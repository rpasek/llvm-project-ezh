//===-- ProcessEZH.cpp ----------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Description:
//   Implements remote target connection, hardware base address resolution
//   RAM-patching software breakpoints, and single stepping.
//
//===----------------------------------------------------------------------===//
#include "ProcessEZH.h"
#include "EZHRegisters.h"
#include "ThreadEZH.h"
#include "lldb/Core/Module.h"
#include "lldb/Utility/State.h"

#include <chrono>
#include <thread>

#include "lldb/Symbol/Function.h"
#include "lldb/Symbol/Symbol.h"
#include "lldb/Symbol/SymbolContext.h"
#include "lldb/Symbol/Symtab.h"
#include "lldb/Symbol/Variable.h"
#include "lldb/Symbol/VariableList.h"

#include "lldb/Breakpoint/Breakpoint.h"
#include "lldb/Breakpoint/BreakpointList.h"
#include "lldb/Breakpoint/BreakpointSite.h"

#include "lldb/Core/Debugger.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Core/UserSettingsController.h"
#include "lldb/Core/Value.h"
#include "lldb/Interpreter/OptionValueProperties.h"
#include "lldb/Interpreter/OptionValueUInt64.h"
#include "lldb/Target/StopInfo.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Utility/RegisterValue.h"
#include "lldb/Utility/Status.h"

#define EZH_DEFAULT_BASE_ADDR 0x40027000

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::process_gdb_remote;

namespace {
class ProcessEZHProperties : public Properties {
public:
  ProcessEZHProperties() : Properties() {
    m_collection_sp =
        std::make_shared<OptionValueProperties>("ezh-remote");
    m_collection_sp->Initialize(PropertyCollectionDefinition{
        llvm::ArrayRef<PropertyDefinition>(g_properties),
        "plugin.process.ezh-remote"});
  }
  static llvm::StringRef GetSettingName() { return "ezh-remote"; }

  lldb::addr_t GetBaseAddress() const {
    const uint32_t idx = 0;
    OptionValueUInt64 *value =
        m_collection_sp->GetPropertyAtIndexAsOptionValueUInt64(idx);
    if (value)
      return value->GetCurrentValue();
    return EZH_DEFAULT_BASE_ADDR;
  }

private:
  static const PropertyDefinition g_properties[1];
};

const PropertyDefinition ProcessEZHProperties::g_properties[1] = {
    {"base-address",
     OptionValue::eTypeUInt64,
     true,
     EZH_DEFAULT_BASE_ADDR,
     nullptr,
     {},
     "The physical base address of the EZH hardware."}};

static ProcessEZHProperties &GetGlobalEZHProperties() {
  static ProcessEZHProperties g_settings;
  return g_settings;
}

static lldb::addr_t LookupGlobalSymbolAddress(Target &target,
                                              const char *name) {
  VariableList var_list;
  target.GetImages().FindGlobalVariables(ConstString(name), 1, var_list);
  if (var_list.GetSize() == 0)
    return LLDB_INVALID_ADDRESS;

  VariableSP var_sp = var_list.GetVariableAtIndex(0);
  if (!var_sp)
    return LLDB_INVALID_ADDRESS;

  ExecutionContext exe_ctx(target);
  auto value_or_error = var_sp->LocationExpressionList().Evaluate(
      &exe_ctx, nullptr, LLDB_INVALID_ADDRESS, nullptr, nullptr);
  if (!value_or_error) {
    llvm::consumeError(value_or_error.takeError());
    return LLDB_INVALID_ADDRESS;
  }

  return value_or_error.get().GetScalar().ULongLong(LLDB_INVALID_ADDRESS);
}
} // namespace

LLDB_PLUGIN_DEFINE_ADV(ProcessEZH, ProcessEZH)

ProcessEZH::ProcessEZH(TargetSP target_sp, ListenerSP listener_sp)
    : ProcessGDBRemote(target_sp, listener_sp) {
  for (int i = 0; i < EZH_MAX_SW_BREAKPOINTS; ++i) {
    m_active_sw_breakpoints[i] = LLDB_INVALID_ADDRESS;
    m_halt_snapshot_sw_breakpoints[i] = LLDB_INVALID_ADDRESS;
  }
  m_polling_thread = std::thread(&ProcessEZH::PollingThread, this);
}

ProcessEZH::~ProcessEZH() {
  m_destroy_polling_thread = true;
  m_polling_cv.notify_all();
  if (m_polling_thread.joinable()) {
    m_polling_thread.join();
  }
}

lldb::addr_t ProcessEZH::GetBaseAddress() const {
  return GetGlobalEZHProperties().GetBaseAddress();
}

Status ProcessEZH::DoConnectRemote(llvm::StringRef remote_url) {
  Status error = ProcessGDBRemote::DoConnectRemote(remote_url);
  if (error.Success()) {
    if (m_async_thread.IsJoinable()) {
      m_async_broadcaster.BroadcastEvent(eBroadcastBitAsyncThreadShouldExit);
      m_async_thread.Join(nullptr);
      m_async_thread.Reset();
    }

    memcpy(m_halt_snapshot_sw_breakpoints, m_active_sw_breakpoints,
           sizeof(m_active_sw_breakpoints));
    SetPrivateState(eStateStopped);
  }
  return error;
}

void ProcessEZH::WillPublicStop() {
  // Do absolutely nothing! Bypass base class GDB remote thread queries to
  // eliminate OpenOCD deprecation warnings.
}

void ProcessEZH::DidAttach(lldb_private::ArchSpec &process_arch) {
  // Do absolutely nothing! Bypass base class DidLaunchOrAttach to prevent
  // sending qProcessInfo, qSymbol, or structured data queries.
}

llvm::Error ProcessEZH::UpdateBreakpointSites(
    const BreakpointSiteToActionMap &site_to_action) {
  return lldb_private::Process::UpdateBreakpointSites(site_to_action);
}

static bool IsOnSamePage(lldb::addr_t src, lldb::addr_t dst) {
  return ((src + EZH_PIPELINE_PC_OFFSET) & EZH_PAGE_BASE_MASK_21BIT) ==
         (dst & EZH_PAGE_BASE_MASK_21BIT);
}

static lldb::addr_t ResolveBreakpointTargetAddr(ProcessEZH &process,
                                                lldb::addr_t bp_addr,
                                                uint32_t slot, Status &error) {
  lldb::addr_t debug_sw_bp_addr = process.GetDebugSoftwareBreakpointAddr(slot);
  if (debug_sw_bp_addr == LLDB_INVALID_ADDRESS) {
    error = Status::FromErrorStringWithFormat(
        "Failed to resolve 'debug_software_breakpoint_%d' exception vector.",
        slot);
    return LLDB_INVALID_ADDRESS;
  }

  if (IsOnSamePage(bp_addr, debug_sw_bp_addr)) {
    return debug_sw_bp_addr;
  }

  char thunk_name[64];
  snprintf(thunk_name, sizeof(thunk_name),
           "__EZHThunk_debug_software_breakpoint_%u", slot);

  lldb::addr_t thunk_addr = LLDB_INVALID_ADDRESS;
  SymbolContextList sc_list;
  process.GetTarget().GetImages().FindSymbolsWithNameAndType(
      ConstString(thunk_name), eSymbolTypeAny, sc_list);

  // Multi-Page Thunk Resolution.
  // We iterate over ALL local thunk symbols across the entire application
  // binary and select the exact one that resides on the same 8MB page as our
  // target breakpoint.
  for (uint32_t i = 0; i < sc_list.GetSize(); ++i) {
    SymbolContext sc;
    if (sc_list.GetContextAtIndex(i, sc)) {
      if (sc.symbol) {
        lldb::addr_t candidate_addr =
            sc.symbol->GetAddress().GetLoadAddress(&process.GetTarget());
        if (candidate_addr == LLDB_INVALID_ADDRESS)
          candidate_addr = sc.symbol->GetAddress().GetFileAddress();
        if (candidate_addr != LLDB_INVALID_ADDRESS &&
            IsOnSamePage(bp_addr, candidate_addr)) {
          thunk_addr = candidate_addr;
          break;
        }
      }
    }
  }

  if (thunk_addr == LLDB_INVALID_ADDRESS) {
    error = Status::FromErrorStringWithFormat(
        "Breakpoint at 0x%" PRIx64 " (Page 0x%" PRIx64
        ") is not on the same 8MB page as the debug handler, "
        "and no pre-allocated thunk '%s' residing on this executable page was "
        "found in the symbol table. "
        "If your application spans multiple 8MB pages, please ensure you place "
        "an EZH_DEFINE_DEBUG_BREAKPOINT_RESERVE() macro invocation "
        "inside each active code page.",
        bp_addr,
        ((bp_addr + EZH_PIPELINE_PC_OFFSET) & EZH_PAGE_BASE_MASK_21BIT),
        thunk_name);
    return LLDB_INVALID_ADDRESS;
  }

  return thunk_addr;
}

Status ProcessEZH::EnableBreakpointSite(BreakpointSite *bp_site) {
  if (!GetTarget().GetExecutableModule())
    return Status::FromErrorString(
        "Cannot enable breakpoint without an active ELF "
        "symbols file loaded.");

  if (!bp_site)
    return Status::FromErrorString("Invalid breakpoint site.");

  addr_t addr = bp_site->GetLoadAddress();

  if (IsBreakpointSiteEnabled(*bp_site))
    return Status();

  // Find if already enabled.
  int slot = -1;
  for (int i = 0; i < EZH_MAX_SW_BREAKPOINTS; ++i)
    if (m_active_sw_breakpoints[i] == addr) {
      slot = i;
      break;
    }

  // If not already enabled, find an empty slot.
  if (slot == -1)
    for (int i = 0; i < EZH_MAX_SW_BREAKPOINTS; ++i)
      if (m_active_sw_breakpoints[i] == LLDB_INVALID_ADDRESS) {
        slot = i;
        break;
      }

  if (slot == -1)
    return Status::FromErrorString("EZH only supports up to 16 active software "
                                   "breakpoints concurrently.");

  // Resolve the correct target address (direct handler or pre-allocated thunk).
  Status resolve_error;
  lldb::addr_t target_branch_addr =
      ResolveBreakpointTargetAddr(*this, addr, slot, resolve_error);
  if (target_branch_addr == LLDB_INVALID_ADDRESS)
    return resolve_error;

  // Read original 4-byte instruction at addr from target RAM.
  uint8_t original_bytes[4];
  Status error;
  size_t bytes_read = DoReadMemoryDirect(addr, original_bytes, 4, error);
  if (bytes_read != 4 || error.Fail())
    return Status::FromErrorStringWithFormat(
        "Failed to read target instruction for software breakpoint backup: %s",
        error.AsCString());

  // Backup the original instruction bytes in bp_site (so LLDB can restore it on
  // hit/disable).
  memcpy(bp_site->GetSavedOpcodeBytes(), original_bytes, 4);
  bp_site->SetType(BreakpointSite::eSoftware);

  // Construct software breakpoint instruction: e_goto target_branch_addr.
  uint32_t sw_bp_op = EncodeGoto(target_branch_addr);

  // Write software breakpoint instruction word directly to target memory
  size_t bytes_written = DoWriteMemory(addr, &sw_bp_op, 4, error);
  if (bytes_written != 4 || error.Fail())
    return Status::FromErrorStringWithFormat(
        "Failed to write software breakpoint trap opcode to target RAM: %s",
        error.AsCString());

  m_active_sw_breakpoints[slot] = addr;
  SetBreakpointSiteEnabled(*bp_site, true);
  return Status();
}

Status ProcessEZH::DisableBreakpointSite(BreakpointSite *bp_site) {
  if (!bp_site)
    return Status::FromErrorString("Invalid breakpoint site.");

  addr_t addr = bp_site->GetLoadAddress();

  if (!IsBreakpointSiteEnabled(*bp_site))
    return Status();

  // Retrieve saved original instruction bytes from bp_site backup.
  const uint8_t *original_bytes = bp_site->GetSavedOpcodeBytes();
  if (!original_bytes)
    return Status::FromErrorString(
        "No backup bytes available to restore original instruction.");

  // Overwrite software breakpoint instruction in RAM with original bytes
  Status error;
  size_t bytes_written = DoWriteMemory(addr, original_bytes, 4, error);
  if (bytes_written != 4 || error.Fail())
    return Status::FromErrorStringWithFormat(
        "Failed to restore target instruction from backup: %s",
        error.AsCString());

  SetBreakpointSiteEnabled(*bp_site, false);
  return Status();
}

void ProcessEZH::Initialize() {
  PluginManager::RegisterPlugin(GetPluginNameStatic(),
                                "GDB Remote EZH coprocessor debugging plugin",
                                CreateInstance, DebuggerInitialize);
}

void ProcessEZH::DebuggerInitialize(Debugger &debugger) {
  if (!PluginManager::GetSettingForProcessPlugin(
          debugger, ProcessEZHProperties::GetSettingName())) {
    const bool is_global_setting = true;
    PluginManager::CreateSettingForProcessPlugin(
        debugger, GetGlobalEZHProperties().GetValueProperties(),
        "Properties for the EZH remote process plugin.", is_global_setting);
  }
}

void ProcessEZH::Terminate() {
  PluginManager::UnregisterPlugin(CreateInstance);
}

ProcessSP ProcessEZH::CreateInstance(TargetSP target_sp, ListenerSP listener_sp,
                                     const FileSpec *crash_file_path,
                                     bool can_connect) {
  ProcessSP process_sp;
  if (crash_file_path == nullptr) {
    if (target_sp->GetArchitecture().GetTriple().getArchName().starts_with(
            "ezh")) {
      process_sp = std::make_shared<ProcessEZH>(target_sp, listener_sp);
    }
  }
  return process_sp;
}

std::shared_ptr<lldb_private::process_gdb_remote::ThreadGDBRemote>
ProcessEZH::CreateThread(lldb::tid_t tid) {
  return std::make_shared<ThreadEZH>(*this, tid);
}

ArchSpec ProcessEZH::GetSystemArchitecture() {
  return GetTarget().GetArchitecture();
}

Status ProcessEZH::DoResume(lldb::RunDirection direction) {
  if (!GetTarget().GetExecutableModule()) {
    return Status::FromErrorString(
        "Cannot resume or single-step EZH co-processor without an active ELF "
        "symbols file loaded");
  }
  m_memory_cache.Clear();

  ThreadSP thread_sp = m_thread_list.GetThreadAtIndex(0);
  if (!thread_sp)
    return Status::FromErrorString("No active thread.");

  RegisterContextSP reg_ctx_sp = thread_sp->GetRegisterContext();
  if (reg_ctx_sp)
    reg_ctx_sp->InvalidateAllRegisters();

  StateType resume_state = thread_sp->GetTemporaryResumeState();
  m_is_stepping = (resume_state == eStateStepping);
  m_halt_requested = false;
  Status error;

  // Resolve and read EZH's RAM handshake variable debug_frame
  addr_t debug_frame_addr_init = GetDebugFrameAddr();

  uint32_t sp_val_init = 0;
  if (debug_frame_addr_init != LLDB_INVALID_ADDRESS)
    DoReadMemory(debug_frame_addr_init, &sp_val_init, EZH_INSTR_SIZE_BYTES,
                 error);

  // Restore the user's true PC address back to stack RAM before unhalting.
  // IMPORTANT: Execute this BEFORE clearing deferred breakpoint slots, because
  // PC restoration inspects m_active_sw_breakpoints.
  if (sp_val_init != 0 && reg_ctx_sp) {
    uint32_t pc_val = reg_ctx_sp->ReadRegisterAsUnsigned(EZH_REG_IDX_PC, 0);
    const RegisterInfo *pc_info =
        reg_ctx_sp->GetRegisterInfoAtIndex(EZH_REG_IDX_PC);
    reg_ctx_sp->WriteRegister(pc_info, RegisterValue(pc_val));
  }

  // Now it is safe to clean up any deferred cleared software breakpoints!
  // Their slots will become free for the single-step logic to reuse.
  for (int i = 0; i < EZH_MAX_SW_BREAKPOINTS; ++i) {
    addr_t addr = m_active_sw_breakpoints[i];
    if (addr != LLDB_INVALID_ADDRESS && addr != m_step_bp_addr) {
      BreakpointSiteSP bp_site_sp = GetBreakpointSiteList().FindByAddress(addr);
      if (!bp_site_sp || !IsBreakpointSiteEnabled(*bp_site_sp))
        m_active_sw_breakpoints[i] = LLDB_INVALID_ADDRESS;
    }
  }

  // Invalidate register cache to force reloading PC from target RAM (where we
  // just restored the user's true instruction address).
  if (reg_ctx_sp)
    reg_ctx_sp->InvalidateAllRegisters();

  // Clear EZH Interrupt 7 pending status to prevent recursive loops on resume.
  // Read current PENDTRAP, and only clear our pending request bit 7
  uint32_t trap_val_clear = 0;
  error = ReadEZHRegister(EZHB_PENDTRAP_OFFSET, trap_val_clear);
  if (error.Fail())
    return error;

  trap_val_clear &= ~(1 << EZHB_PENDTRAP_REQ7);
  error = WriteEZHRegister(EZHB_PENDTRAP_OFFSET, trap_val_clear);
  if (error.Fail())
    return error;

  // Prevent stepping actively executing targets that have no stack frame!
  if (resume_state == eStateStepping && sp_val_init == 0) {
    uint32_t ctrl_val_check = 0;
    error = ReadEZHRegister(EZHB_CTRL_OFFSET, ctrl_val_check);
    if (error.Fail())
      return error;

    if ((ctrl_val_check & (1 << EZHB_START)) != 0) {
      if (thread_sp)
        thread_sp->DiscardThreadPlansUpToPlan(thread_sp->GetCurrentPlan());
      return Status::FromErrorString(
          "Cannot single-step an actively executing target. Halt it first.");
    }
  }

  if (resume_state == eStateStepping) {
    // Check if EZH is physically stopped/reset (Start = 0).
    uint32_t ctrl_val = 0;
    error = ReadEZHRegister(EZHB_CTRL_OFFSET, ctrl_val);
    if (error.Fail())
      return error;

    if ((ctrl_val & (1 << EZHB_START)) == 0) {
      if (thread_sp)
        thread_sp->DiscardThreadPlansUpToPlan(thread_sp->GetCurrentPlan());
      return Status::FromErrorString(
          "Cannot step, stepi, next or finish a non running core (Start = 0) "
          "because the stack pointer (SP) has not been initialized yet. Please "
          "set a breakpoint after stack initialization and use 'continue' to "
          "start the core.");
    }

    // Read active PC from register context
    RegisterContextSP reg_ctx_sp = thread_sp->GetRegisterContext();
    uint32_t pc_val = 0;
    if (reg_ctx_sp)
      pc_val = reg_ctx_sp->ReadRegisterAsUnsigned(EZH_REG_IDX_PC, 0);

    if (!ResolveSoftwareBreakpointSentinel(pc_val, error))
      return error;

    // Decode EZH instruction at current PC to predict next PC (branch target
    // prediction)
    uint32_t next_pc = pc_val + EZH_INSTR_SIZE_BYTES; // Sequential fallback
    uint32_t inst_val = 0;
    DoReadMemory(pc_val, &inst_val, EZH_INSTR_SIZE_BYTES, error);
    if (error.Fail())
      return error;

    // Read active ALU flags register directly from RAM frame for predictions
    uint32_t flags = 0;
    if (sp_val_init != 0) {
      DoReadMemory(sp_val_init + EZH_FRAME_OFFSET_FLAGS, &flags,
                   EZH_INSTR_SIZE_BYTES, error);
      if (error.Fail())
        return error;
    }

    next_pc = PredictPCDestination(inst_val, pc_val, reg_ctx_sp.get(),
                                   flags, error);
    if (error.Fail())
      return error;

    // Set temporary Software Breakpoint (RAM patch) at next PC to prevent
    // prefetch PC corruption hazards!
    bool rn_wb = false;
    uint32_t rn_val = 0;
    fprintf(
        stderr,
        "[ProcessEZH::DoResume] SINGLE_STEP: PC=0x%08x, inst=0x%08x -> "
        "Predicted next_pc=0x%08x (sp_init=0x%08x, rn_wb=%u, rn_val=0x%08x)\n",
        pc_val, inst_val, next_pc, sp_val_init, rn_wb, rn_val);

    // Clean up any stale step breakpoint first
    if (m_step_bp_addr != LLDB_INVALID_ADDRESS) {
      DoWriteMemory(m_step_bp_addr, &m_step_bp_original_op,
                    EZH_INSTR_SIZE_BYTES, error);
      if (m_step_bp_slot != -1)
        m_active_sw_breakpoints[m_step_bp_slot] = LLDB_INVALID_ADDRESS;
      m_step_bp_addr = LLDB_INVALID_ADDRESS;
      m_step_bp_original_op = 0;
      m_step_bp_slot = -1;
    }

    // Find an empty slot for the step breakpoint.
    int slot = -1;
    for (int i = 0; i < EZH_MAX_SW_BREAKPOINTS; ++i)
      if (m_active_sw_breakpoints[i] == LLDB_INVALID_ADDRESS) {
        slot = i;
        break;
      }

    if (slot == -1)
      return Status::FromErrorString(
          "Cannot single-step EZH target: All 16 software breakpoint slots are "
          "currently full. Please disable a breakpoint first.");

    Status resolve_error;
    lldb::addr_t target_branch_addr =
        ResolveBreakpointTargetAddr(*this, next_pc, slot, resolve_error);
    if (target_branch_addr == LLDB_INVALID_ADDRESS)
      return resolve_error;

    // Read original instruction at next_pc.
    uint32_t original_op = 0;
    size_t bytes_read = DoReadMemoryDirect(next_pc, &original_op, 4, error);
    if (bytes_read != 4 || error.Fail())
      return Status::FromErrorStringWithFormat(
          "Failed to read original instruction at single-step target 0x%08llx: "
          "%s",
          (unsigned long long)next_pc, error.AsCString());

    // Backup original state.
    m_step_bp_addr = next_pc;
    m_step_bp_original_op = original_op;
    m_step_bp_slot = slot;
    m_active_sw_breakpoints[slot] = next_pc;

    // Construct software breakpoint instruction: goto target_branch_addr.
    uint32_t sw_bp_op = EncodeGoto(target_branch_addr);

    // Patch RAM.
    size_t bytes_written = DoWriteMemory(next_pc, &sw_bp_op, 4, error);
    if (bytes_written != 4 || error.Fail()) {
      // Rollback backup state on write error.
      m_active_sw_breakpoints[slot] = LLDB_INVALID_ADDRESS;
      m_step_bp_addr = LLDB_INVALID_ADDRESS;
      m_step_bp_original_op = 0;
      m_step_bp_slot = -1;
      return Status::FromErrorStringWithFormat(
          "Failed to patch RAM at single-step target 0x%08llx: %s",
          (unsigned long long)next_pc, error.AsCString());
    }

    // Unhalt EZH co-processor: set debug_frame in RAM to 0
    uint32_t zero = 0;
    if (debug_frame_addr_init != LLDB_INVALID_ADDRESS) {
      WriteMemory(debug_frame_addr_init, &zero, 4, error);
      if (error.Fail())
        return error;
      // Give the adapter 10ms to process the unhalt cleanly.
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  } else if (resume_state == eStateRunning) {
    // Check the physical EZH ignition state first!
    uint32_t ctrl_val = 0;
    error = ReadEZHRegister(EZHB_CTRL_OFFSET, ctrl_val);
    if (error.Fail())
      return error;
    bool ignition_set = ((ctrl_val & (1 << EZHB_START)) != 0);

    if (!ignition_set) {
      // EZH is physically stopped/reset (Start = 0). Ignite it!

      // Invalidate cached m_debug_frame_addr to guarantee reload
      m_debug_frame_addr = LLDB_INVALID_ADDRESS;

      // Clear EZH's RAM handshake variable debug_frame over debug wire first.
      uint32_t zero = 0;
      if (debug_frame_addr_init != LLDB_INVALID_ADDRESS) {
        WriteMemory(debug_frame_addr_init, &zero, EZH_INSTR_SIZE_BYTES,
                    error);
        if (error.Fail())
          return error;
      }

      // Clear Trap 7 request and arm Trap Enable 7 in hardware.
      error = WriteEZHRegister(EZHB_PENDTRAP_OFFSET,
                               (1 << EZHB_PENDTRAP_EN7));
      if (error.Fail())
        return error;

      // Automatically resolve EZH's entry point address from the ELF
      // headers.
      auto entry_point_address = GetTarget().GetEntryPointAddress();
      if (entry_point_address) {
        addr_t entry_point =
            entry_point_address->GetLoadAddress(&GetTarget());
        if (entry_point == LLDB_INVALID_ADDRESS)
          entry_point =
              entry_point_address
                  ->GetFileAddress(); // Robust static File Address fallback!
        if (entry_point != LLDB_INVALID_ADDRESS) {
          error = WriteEZHRegister(EZHB_BOOT_OFFSET,
                                   static_cast<uint32_t>(entry_point));
          if (error.Fail())
            return error;
        }
      } else {
        llvm::consumeError(entry_point_address.takeError());
      }

      // Ignite EZH!
      uint32_t ctrl_ignite =
          EZHB_CTRL_WRITE_KEY | (ctrl_val & 0xFFFF) | (1 << EZHB_START);
      error = WriteEZHRegister(EZHB_CTRL_OFFSET, ctrl_ignite);
      if (error.Fail())
        return error;
      m_is_stepping = false;
    } else {
      // EZH is physically ignited (Start = 1). Only unhalt if we have a valid
      // stack frame.
      if (sp_val_init != 0) {
        // EZH is in Suspended / Halted state. Unhalt EZH: set debug_frame in
        // RAM to 0.
        uint32_t zero = 0;
        if (debug_frame_addr_init != LLDB_INVALID_ADDRESS) {
          WriteMemory(debug_frame_addr_init, &zero, EZH_INSTR_SIZE_BYTES,
                      error);
          if (error.Fail())
            return error;
        }
      }
    }
  }

  // Transition state to eStateRunning so polling thread runs and LLDB waits
  // correctly
  SetPrivateState(eStateRunning);

  // Notify polling thread to start polling
  m_polling_cv.notify_one();

  return Status();
}

bool ProcessEZH::ResolveSoftwareBreakpointSentinel(uint32_t &pc_val,
                                                   Status &error) {
  if ((pc_val & EZH_SW_BP_MAGIC_PC_MASK) == EZH_SW_BP_MAGIC_PC_MASK) {
    uint32_t bp_slot = pc_val & EZH_SW_BP_SLOT_MASK;
    if (bp_slot < EZH_MAX_SW_BREAKPOINTS) {
      if (m_halt_snapshot_sw_breakpoints[bp_slot] != LLDB_INVALID_ADDRESS) {
        pc_val = m_halt_snapshot_sw_breakpoints[bp_slot];
      } else {
        error = Status::FromErrorStringWithFormat(
            "Cannot resolve software breakpoint hit: PC contains sentinel "
            "0x%08x for slot %u, but m_halt_snapshot_sw_breakpoints[%u] "
            "does not track a valid address.",
            pc_val, bp_slot, bp_slot);
        return false;
      }
    }
  }
  return true;
}

uint32_t ProcessEZH::PredictPCDestination(uint32_t inst_val, uint32_t pc_val,
                                          RegisterContext *reg_ctx,
                                          uint32_t flags, Status &error) {
  uint32_t fallback = pc_val + EZH_INSTR_SIZE_BYTES;
  if (!reg_ctx)
    return fallback;

  // Check gosub (subroutine call encoded in low 2 bits)
  if ((inst_val & EZH_OPC_MASK_2BIT) == EZH_OPC_GOSUB) {
    EZHGosub op = DecodeGosub(inst_val);
    return op.target_address;
  }

  uint32_t base_opc = inst_val & EZH_OPC_MASK_5BIT;

  // Check goto / gotol / goto_reg conditional branches
  if (base_opc == EZH_OPC_GOTO) {
    EZHGoto op = DecodeGoto(inst_val);
    bool condition_met =
        (op.cond == EZH_CC_EU) || ((flags & (1 << op.cond)) != 0);

    if (condition_met) {
      if (op.is_reg)
        return reg_ctx->ReadRegisterAsUnsigned(op.rs1, fallback);
      return ((pc_val + EZH_PIPELINE_PC_OFFSET) & EZH_PAGE_BASE_MASK_21BIT) |
             (op.imm21 << EZH_WORD_TO_BYTE_SHIFT);
    }
    return fallback;
  }

  // If instruction does not branch and destination is not PC, advance by 4
  uint32_t dest_reg =
      (inst_val >> EZH_INSTR_REG_DEST_SHIFT) & EZH_REG_MASK_4BIT;
  if (dest_reg != EZH_REG_IDX_PC)
    return fallback;

  uint32_t val = fallback;

  // Base Opcode Dispatch (Switch-Case ALU Coverage)
  switch (base_opc) {
  case EZH_OPC_MOV: {
    EZHMov op = DecodeMov(inst_val);
    if (op.is_imm) val = op.imm11;
    else val = reg_ctx->ReadRegisterAsUnsigned(op.rs1, fallback);
    break;
  }
  case EZH_OPC_LDR: {
    EZHLdr op = DecodeLdr(inst_val);
    uint32_t rn_val = reg_ctx->ReadRegisterAsUnsigned(op.rn, 0);
    int32_t effective_offset = op.is_post ? 0 : op.offset;
    DoReadMemory(rn_val + effective_offset, &val, EZH_INSTR_SIZE_BYTES, error);
    if (error.Fail())
      return fallback;
    break;
  }
  case EZH_OPC_PER_READ: {
    EZHPerRead op = DecodePerRead(inst_val);
    DoReadMemory(op.per_addr, &val, EZH_INSTR_SIZE_BYTES, error);
    if (error.Fail())
      return fallback;
    break;
  }
  case EZH_OPC_ANDOR: {
    EZHAndOr op = DecodeAndOr(inst_val);
    uint32_t val1 = reg_ctx->ReadRegisterAsUnsigned(op.rs1, 0);
    uint32_t val2 = reg_ctx->ReadRegisterAsUnsigned(op.rs2, 0);
    uint32_t val3 = reg_ctx->ReadRegisterAsUnsigned(op.rs3, 0);
    return (val1 & val2) | val3;
  }
  case EZH_OPC_ADD:
  case EZH_OPC_SUB:
  case EZH_OPC_ADC:
  case EZH_OPC_SBC:
  case EZH_OPC_AND:
  case EZH_OPC_OR:
  case EZH_OPC_XOR: {
    EZHAlu op = DecodeAlu(inst_val);
    uint32_t val1 = reg_ctx->ReadRegisterAsUnsigned(op.rs1, 0);
    uint32_t val2 = 0;
    if (op.is_imm) val2 = op.imm12;
    else val2 = reg_ctx->ReadRegisterAsUnsigned(op.rs2, 0);

    uint32_t carry_bit = (flags & (1 << EZH_CC_CA)) ? 1 : 0;
    if (base_opc == EZH_OPC_ADD) val = val1 + val2;
    else if (base_opc == EZH_OPC_SUB) val = val1 - val2;
    else if (base_opc == EZH_OPC_ADC) val = val1 + val2 + carry_bit;
    else if (base_opc == EZH_OPC_SBC) val = val1 - val2 - carry_bit;
    else if (base_opc == EZH_OPC_AND) val = val1 & val2;
    else if (base_opc == EZH_OPC_OR) val = val1 | val2;
    else val = val1 ^ val2;
    break;
  }
  case EZH_OPC_MEM_REG: {
    EZHMemReg op = DecodeMemReg(inst_val);
    uint32_t rn_val = reg_ctx->ReadRegisterAsUnsigned(op.rn, 0);
    uint32_t rm_val = reg_ctx->ReadRegisterAsUnsigned(op.rm, 0);
    DoReadMemory(rn_val + rm_val, &val, EZH_INSTR_SIZE_BYTES, error);
    if (error.Fail())
      return fallback;
    break;
  }
  case EZH_OPC_REG_SHIFT: {
    EZHRegShift op = DecodeRegShift(inst_val);
    uint32_t val1 = reg_ctx->ReadRegisterAsUnsigned(op.rs, 0);
    uint32_t sh_amt = reg_ctx->ReadRegisterAsUnsigned(op.rshift, 0) & 0x1F;
    if (op.sh_type == EZH_SHIFT_TYPE_LSL) val = val1 << sh_amt;
    else if (op.sh_type == EZH_SHIFT_TYPE_ASR)
      val = static_cast<uint32_t>(static_cast<int32_t>(val1) >> sh_amt);
    else if (op.sh_type == EZH_SHIFT_TYPE_LSR) val = val1 >> sh_amt;
    else val = (val1 >> sh_amt) | (val1 << (32 - sh_amt));
    break;
  }
  case EZH_OPC_BIT: {
    EZHBit op = DecodeBit(inst_val);
    uint32_t val1 = reg_ctx->ReadRegisterAsUnsigned(op.rs, fallback);
    uint32_t bit_idx = op.imm5;
    if (op.is_reg) {
      bit_idx = reg_ctx->ReadRegisterAsUnsigned(op.rbit, 0) & 0x1F;
    }
    if (op.op_type == EZH_BIT_OP_CLEAR) val = val1 & ~(1 << bit_idx);
    else if (op.op_type == EZH_BIT_OP_SET) val = val1 | (1 << bit_idx);
    else if (op.op_type == EZH_BIT_OP_TOGGLE) val = val1 ^ (1 << bit_idx);
    else val = val1;
    break;
  }
  case EZH_OPC_FLIP: {
    EZHFlip op = DecodeFlip(inst_val);
    uint32_t val1 = reg_ctx->ReadRegisterAsUnsigned(op.rs, fallback);
    val = ((val1 & 0x000000FF) << 24) | ((val1 & 0x0000FF00) << 8) |
          ((val1 & 0x00FF0000) >> 8) | ((val1 & 0xFF000000) >> 24);
    break;
  }
  default: {
    return fallback;
  }
  }

  // Universal ISA Post-Modifiers (Invert & Barrel Shift apply only to ALU opcodes)
  if (base_opc == EZH_OPC_ADD || base_opc == EZH_OPC_SUB ||
      base_opc == EZH_OPC_ADC || base_opc == EZH_OPC_SBC ||
      base_opc == EZH_OPC_AND || base_opc == EZH_OPC_OR ||
      base_opc == EZH_OPC_XOR || base_opc == EZH_OPC_ANDOR) {
    EZHModifiers mods = DecodeModifiers(inst_val);
    if (mods.invert_result)
      val = ~val;

    if (mods.shift_amount != 0) {
      switch (mods.shift_type) {
      case EZH_SHIFT_TYPE_LSL:
        val = val << mods.shift_amount;
        break;
      case EZH_SHIFT_TYPE_ASR:
        val = static_cast<uint32_t>(
            static_cast<int32_t>(val) >> mods.shift_amount);
        break;
      case EZH_SHIFT_TYPE_LSR:
        val = val >> mods.shift_amount;
        break;
      case EZH_SHIFT_TYPE_ROR:
        val = (val >> mods.shift_amount) | (val << (32 - mods.shift_amount));
        break;
      }
    }
  }

  // If the predicted destination PC targets a software breakpoint sentinel
  // (e.g. unstacking a return address via popd pc upon exiting an interrupt or
  // subroutine that originally interrupted an active breakpoint), resolve the
  // sentinel to the tracked breakpoint address so single-step trap patching
  // operates on physical memory.
  if (!ResolveSoftwareBreakpointSentinel(val, error))
    return fallback;

  return val;
}

bool ProcessEZH::DoUpdateThreadList(ThreadList &old_thread_list,
                                    ThreadList &new_thread_list) {
  // Preserve EZH's real target thread context if it exists to keep stop info and
  // register contexts intact. Pass false to GetThreadAtIndex to prevent
  // infinite recursive thread list update checking.
  ThreadSP thread_sp = old_thread_list.GetThreadAtIndex(0, false);
  if (!thread_sp)
    thread_sp = std::make_shared<ThreadEZH>(*this, 1);
  new_thread_list.AddThread(thread_sp);
  return true;
}

Status ProcessEZH::DoHalt(bool &caused_stop) {
  if (m_is_stepping)
    // If EZH is actively single-stepping, do NOT inject Trap 7. Let the
    // hardware hit the breakpoint naturally.
    return Status();
  m_halt_requested = true;
  caused_stop = false;

  // Read current PENDTRAP to preserve other active traps.
  uint32_t trap_val = 0;
  Status error;
  error = ReadEZHRegister(EZHB_PENDTRAP_OFFSET, trap_val);
  if (error.Fail())
    return error;

  // Enable pending trap 7 (bit 23) and set request (bit 7) safely via OR
  trap_val |= (1 << EZHB_PENDTRAP_EN7) | (1 << EZHB_PENDTRAP_REQ7);

  error = WriteEZHRegister(EZHB_PENDTRAP_OFFSET, trap_val);
  if (error.Fail())
    return Status::FromErrorStringWithFormat(
        "Failed to write EZH PENDTRAP register: %s",
        error.AsCString());

  caused_stop = true;
  return Status();
}

void ProcessEZH::RefreshStateAfterStop() {
  Status error;
  // Clean up temporary Software Single-Step breakpoint if it was active.
  if (m_step_bp_addr != LLDB_INVALID_ADDRESS) {
    DoWriteMemory(m_step_bp_addr, &m_step_bp_original_op,
                  EZH_INSTR_SIZE_BYTES, error);

    // Force commit user's true PC back to stack RAM to replace the
    // breakpoint marker immediately. This allows us to free the step BP slot
    // right now, instead of waiting for DoResume.
    lldb::addr_t debug_frame_addr = GetDebugFrameAddr();
    if (debug_frame_addr != LLDB_INVALID_ADDRESS) {
      uint32_t sp_val = 0;
      DoReadMemory(debug_frame_addr, &sp_val, EZH_INSTR_SIZE_BYTES, error);
      if (sp_val != 0) {
        uint32_t pc_val = static_cast<uint32_t>(m_step_bp_addr);
        // Write user PC to active stack frame in RAM
        DoWriteMemory(sp_val + EZH_FRAME_OFFSET_PC, &pc_val,
                      EZH_INSTR_SIZE_BYTES, error);
      }
    }

    // Now we can safely free the slot immediately.
    if (m_step_bp_slot != -1)
      m_active_sw_breakpoints[m_step_bp_slot] = LLDB_INVALID_ADDRESS;

    m_step_bp_addr = LLDB_INVALID_ADDRESS;
    m_step_bp_original_op = 0;
    m_step_bp_slot = -1;
  }

  // Wait for EZH hardware trap handler to finish dumping registers to RAM.
  lldb::addr_t debug_frame_addr = GetDebugFrameAddr();
  if (debug_frame_addr != LLDB_INVALID_ADDRESS) {
    uint32_t sp_val = 0;
    Status error;
    m_memory_cache.Clear();
    DoReadMemory(debug_frame_addr, &sp_val, EZH_INSTR_SIZE_BYTES, error);
    if (sp_val == 0)
      for (int i = 0; i < 20; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        m_memory_cache.Clear();
        DoReadMemory(debug_frame_addr, &sp_val, EZH_INSTR_SIZE_BYTES, error);
        if (sp_val != 0)
          break;
      }
  }

  // Refresh thread list directly without invoking base class GDB remote thread
  // queries
  m_thread_ids.clear();
  m_thread_pcs.clear();
  UpdateThreadListIfNeeded();
  if (m_last_stop_packet) {
    SetThreadStopInfo(*m_last_stop_packet);
    m_last_stop_packet.reset();
  }
  m_thread_list_real.RefreshStateAfterStop();

  ThreadSP thread_sp = m_thread_list.GetThreadAtIndex(0);
  if (thread_sp) {
    RegisterContextSP reg_ctx_sp = thread_sp->GetRegisterContext();
    if (reg_ctx_sp)
      reg_ctx_sp->InvalidateAllRegisters();

    StateType temp_state = thread_sp->GetTemporaryResumeState();
    if (temp_state == eStateStepping) {
      thread_sp->SetStopInfo(StopInfo::CreateStopReasonToTrace(*thread_sp));
    } else {
      addr_t pc_val = 0;
      if (reg_ctx_sp)
        pc_val = reg_ctx_sp->ReadRegisterAsUnsigned(EZH_REG_IDX_PC, 0ULL);
      BreakpointSiteSP bp_site_sp =
          GetBreakpointSiteList().FindByAddress(pc_val);
      if (bp_site_sp)
        thread_sp->SetStopInfo(StopInfo::CreateStopReasonWithBreakpointSiteID(
            *thread_sp, bp_site_sp->GetID()));
      else
        thread_sp->SetStopInfo(StopInfo::CreateStopReasonToTrace(*thread_sp));
    }
  }
}

void ProcessEZH::ModulesDidLoad(lldb_private::ModuleList &module_list) {
  // Call base class to process the newly loaded modules first.
  ProcessGDBRemote::ModulesDidLoad(module_list);

  // Automatically resolve ELF entrypoint address and load it into hardware
  // BOOTADR register.
  auto entry_point_address = GetTarget().GetEntryPointAddress();
  if (entry_point_address) {
    addr_t entry_point = entry_point_address->GetLoadAddress(&GetTarget());
    if (entry_point == LLDB_INVALID_ADDRESS)
      entry_point =
          entry_point_address
              ->GetFileAddress();
    if (entry_point != LLDB_INVALID_ADDRESS)
      WriteEZHRegister(EZHB_BOOT_OFFSET, (uint32_t)entry_point);
  } else {
    llvm::consumeError(entry_point_address.takeError());
  }
}

void ProcessEZH::PollingThread() {
  lldb_private::Status error;

  while (!m_destroy_polling_thread) {
    std::unique_lock<std::mutex> lock(m_polling_mutex);
    m_polling_cv.wait(lock, [this]() {
      return GetPrivateState() == lldb::eStateRunning ||
             m_destroy_polling_thread;
    });

    if (m_destroy_polling_thread)
      break;

    lock.unlock();

    uint32_t sp_val = 0;
    lldb::addr_t exc_signal_addr = LLDB_INVALID_ADDRESS;
    bool first_poll = true;
    int poll_count = 0;

    while (GetPrivateState() == lldb::eStateRunning &&
           !m_destroy_polling_thread) {
      poll_count++;
      if (first_poll) {
        if (m_is_stepping)
          std::this_thread::sleep_for(std::chrono::milliseconds(
              5)); // Initial 5ms wait for stepping to complete on target
      } else {
        if (m_is_stepping)
          std::this_thread::sleep_for(
              std::chrono::milliseconds(5)); // 5ms poll for single-stepping
        else
          std::this_thread::sleep_for(std::chrono::milliseconds(
              500)); // Standard 500ms poll for continuous execution
      }
      first_poll = false;

      if (m_destroy_polling_thread)
        break;

      size_t bytes_read = 0;
      uint32_t exc_val = 0;
      if (m_debug_frame_addr != LLDB_INVALID_ADDRESS &&
          !m_destroy_polling_thread) {
        m_memory_cache.Clear();
        bytes_read = DoReadMemory(m_debug_frame_addr, &sp_val, 4, error);
      }
      if (exc_signal_addr != LLDB_INVALID_ADDRESS &&
          !m_destroy_polling_thread) {
        Status exc_error;
        m_memory_cache.Clear();
        DoReadMemory(exc_signal_addr, &exc_val, 4, exc_error);
      }

      if (m_debug_frame_addr == LLDB_INVALID_ADDRESS)
        GetDebugFrameAddr();

      if (exc_signal_addr == LLDB_INVALID_ADDRESS)
        exc_signal_addr =
            LookupGlobalSymbolAddress(GetTarget(), "exc_signal");

      if (bytes_read == 4 && error.Success() && sp_val != 0) {
        m_halt_requested = false;
        memcpy(m_halt_snapshot_sw_breakpoints, m_active_sw_breakpoints,
               sizeof(m_active_sw_breakpoints));
        SetPrivateState(lldb::eStateStopped);
        break;
      }
    }
  }
}

Status ProcessEZH::WriteEZHRegister(addr_t offset, uint32_t value) {
  Status error;
  WriteMemory(GetBaseAddress() + offset, &value, 4, error);
  return error;
}

Status ProcessEZH::ReadEZHRegister(addr_t offset, uint32_t &value) {
  Status error;
  m_memory_cache.Clear();
  ReadMemory(GetBaseAddress() + offset, &value, 4, error);
  return error;
}

lldb::addr_t ProcessEZH::GetDebugFrameAddr() {
  if (m_debug_frame_addr == LLDB_INVALID_ADDRESS)
    m_debug_frame_addr = LookupGlobalSymbolAddress(GetTarget(), "debug_frame");
  return m_debug_frame_addr;
}

lldb::addr_t ProcessEZH::GetDebugSoftwareBreakpointAddr(uint32_t slot) {
  if (slot >= EZH_MAX_SW_BREAKPOINTS)
    return LLDB_INVALID_ADDRESS;

  char sym_name[64];
  snprintf(sym_name, sizeof(sym_name), "debug_software_breakpoint_%u", slot);
  SymbolContextList sc_list;
  GetTarget().GetImages().FindSymbolsWithNameAndType(ConstString(sym_name),
                                                     eSymbolTypeAny, sc_list);
  SymbolContext sc;
  if (sc_list.GetSize() > 0 && sc_list.GetContextAtIndex(0, sc) && sc.symbol) {
    lldb::addr_t addr = sc.symbol->GetAddress().GetLoadAddress(&GetTarget());
    if (addr == LLDB_INVALID_ADDRESS)
      addr = sc.symbol->GetAddress().GetFileAddress();
    return addr;
  }
  return LLDB_INVALID_ADDRESS;
}

Status ProcessEZH::DoDetach(bool keep_stopped) {
  // Restore user's true PC back to stack RAM before detaching so target
  // is left in clean state.
  ThreadSP thread_sp = m_thread_list.GetThreadAtIndex(0);
  if (thread_sp) {
    RegisterContextSP reg_ctx_sp = thread_sp->GetRegisterContext();
    if (reg_ctx_sp) {
      uint32_t pc_val = reg_ctx_sp->ReadRegisterAsUnsigned(EZH_REG_IDX_PC, 0);
      if (pc_val != 0 && pc_val != static_cast<uint32_t>(LLDB_INVALID_ADDRESS)) {
        const RegisterInfo *pc_info =
            reg_ctx_sp->GetRegisterInfoAtIndex(EZH_REG_IDX_PC);
        reg_ctx_sp->WriteRegister(pc_info, RegisterValue(pc_val));
      }
    }
  }
  return ProcessGDBRemote::DoDetach(keep_stopped);
}

Status ProcessEZH::DoDestroy() {
  // Restore user's true PC back to stack RAM before destroying so target
  // is left in clean state.
  ThreadSP thread_sp = m_thread_list.GetThreadAtIndex(0);
  if (thread_sp) {
    RegisterContextSP reg_ctx_sp = thread_sp->GetRegisterContext();
    if (reg_ctx_sp) {
      uint32_t pc_val = reg_ctx_sp->ReadRegisterAsUnsigned(EZH_REG_IDX_PC, 0);
      if (pc_val != 0 && pc_val != static_cast<uint32_t>(LLDB_INVALID_ADDRESS)) {
        const RegisterInfo *pc_info =
            reg_ctx_sp->GetRegisterInfoAtIndex(EZH_REG_IDX_PC);
        reg_ctx_sp->WriteRegister(pc_info, RegisterValue(pc_val));
      }
    }
  }
  return ProcessGDBRemote::DoDestroy();
}
