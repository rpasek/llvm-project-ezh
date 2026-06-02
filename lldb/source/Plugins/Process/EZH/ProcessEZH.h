//===-- ProcessEZH.h --------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Description:
//   Declares ProcessEZH, subclassing ProcessGDBRemote to manage EZH remote
//   target execution, RAM patching, software breakpoints, and polling threads.
//
//===----------------------------------------------------------------------===//
#ifndef liblldb_ProcessEZH_h_
#define liblldb_ProcessEZH_h_

#include "../gdb-remote/ProcessGDBRemote.h"
#include "lldb/lldb-private.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "EZHRegisters.h"

class ProcessEZH : public lldb_private::process_gdb_remote::ProcessGDBRemote {
public:
  ProcessEZH(lldb::TargetSP target_sp, lldb::ListenerSP listener_sp);

  ~ProcessEZH() override;

  lldb_private::Status
  EnableBreakpointSite(lldb_private::BreakpointSite *bp_site) override;

  lldb_private::Status
  DisableBreakpointSite(lldb_private::BreakpointSite *bp_site) override;

  llvm::Error UpdateBreakpointSites(
      const lldb_private::Process::BreakpointSiteToActionMap &site_to_action)
      override;

  lldb::addr_t GetBaseAddress() const;

  lldb_private::Status DoConnectRemote(llvm::StringRef remote_url) override;

  void WillPublicStop() override;

  void DidAttach(lldb_private::ArchSpec &process_arch) override;

  void InvalidateMemoryCache() { m_memory_cache.Clear(); }
  size_t DoReadMemoryDirect(lldb::addr_t addr, void *buf, size_t size,
                            lldb_private::Status &error) {
    return DoReadMemory(addr, buf, size, error);
  }

  lldb_private::ArchSpec GetSystemArchitecture() override;

  static void Initialize();

  static void DebuggerInitialize(lldb_private::Debugger &debugger);

  static void Terminate();

  static lldb::ProcessSP
  CreateInstance(lldb::TargetSP target_sp, lldb::ListenerSP listener_sp,
                 const lldb_private::FileSpec *crash_file_path,
                 bool can_connect);

  std::shared_ptr<lldb_private::process_gdb_remote::ThreadGDBRemote>
  CreateThread(lldb::tid_t tid) override;

  bool DoUpdateThreadList(lldb_private::ThreadList &old_thread_list,
                          lldb_private::ThreadList &new_thread_list) override;

  lldb_private::Status DoResume(lldb::RunDirection direction) override;

  lldb_private::Status DoHalt(bool &caused_stop) override;

  lldb_private::Status DoDetach(bool keep_stopped) override;

  lldb_private::Status DoDestroy() override;

  void RefreshStateAfterStop() override;

  static llvm::StringRef GetPluginNameStatic() { return "ezh-remote"; }

  llvm::StringRef GetPluginName() override { return GetPluginNameStatic(); }

  void ModulesDidLoad(lldb_private::ModuleList &module_list) override;

  lldb_private::Status WriteEZHRegister(lldb::addr_t offset, uint32_t value);
  lldb_private::Status ReadEZHRegister(lldb::addr_t offset, uint32_t &value);

  lldb::addr_t GetDebugFrameAddr();
  lldb::addr_t ResolveSentinelPC(lldb::addr_t sentinel_pc);
  void SnapshotSoftwareBreakpoints();

private:
  void PollingThread();
  uint32_t PredictPCDestination(uint32_t inst_val, uint32_t pc_val,
                                lldb_private::RegisterContext *reg_ctx,
                                uint32_t flags, lldb_private::Status &error);
  bool ResolveSoftwareBreakpointSentinel(uint32_t &pc_val,
                                         lldb_private::Status &error);
  void DiscoverBreakpointRegions(lldb_private::SectionList *section_list);

  std::thread m_polling_thread;
  std::atomic<bool> m_destroy_polling_thread{false};
  std::atomic<bool> m_is_stepping{false};
  std::atomic<bool> m_halt_requested{false};
  std::condition_variable m_polling_cv;
  std::mutex m_polling_mutex;

  struct EZHBreakpointSlot {
    lldb::addr_t bp_addr = LLDB_INVALID_ADDRESS;
  };
  struct EZHBreakpointRegion {
    std::string name;
    lldb::addr_t start_addr = LLDB_INVALID_ADDRESS;
    size_t size = 0;
    std::vector<EZHBreakpointSlot> slots;
    // Frozen snapshot of breakpoint slots taken at the moment of halt.
    // This allows LLDB to resolve the PC back to the user's code address
    // even if LLDB clears the active 'slots' entry when disabling the
    // breakpoint site during a resume/step transition before the target
    // actually executes the first step instruction.
    std::vector<EZHBreakpointSlot> halt_snapshot_slots;
  };

  lldb::addr_t m_debug_frame_addr = LLDB_INVALID_ADDRESS;
  std::vector<EZHBreakpointRegion> m_breakpoint_regions;
  lldb::addr_t m_step_bp_addr = LLDB_INVALID_ADDRESS;
  uint32_t m_step_bp_original_op = 0;
  int m_step_bp_region_idx = -1;
  int m_step_bp_slot = -1;
};

#endif // liblldb_ProcessEZH_h_
