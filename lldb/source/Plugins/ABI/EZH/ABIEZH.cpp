//===-- ABIEZH.cpp --------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIEZH.h"

#include <array>

#include "lldb/Core/PluginManager.h"
#include "lldb/Core/Value.h"
#include "lldb/Symbol/CompilerType.h"
#include "lldb/Symbol/UnwindPlan.h"
#include "lldb/Target/RegisterContext.h"
#include "lldb/Target/StackFrame.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/RegisterValue.h"
#include "lldb/Utility/Scalar.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Error.h"

#define DEFINE_REG_NAME(reg_num)                                               \
  lldb_private::ConstString(#reg_num).GetCString()
#define DEFINE_REG_NAME_STR(reg_name)                                          \
  lldb_private::ConstString(reg_name).GetCString()

#define DEFINE_GENERIC_REGISTER_STUB(dwarf_num, str_name, generic_num)         \
  {                                                                            \
      DEFINE_REG_NAME(dwarf_num),                                              \
      DEFINE_REG_NAME_STR(str_name),                                           \
      4,                                                                       \
      0,                                                                       \
      lldb::eEncodingUint,                                                     \
      lldb::eFormatHex,                                                        \
      {dwarf_num, dwarf_num, generic_num, dwarf_num, dwarf_num},               \
      nullptr,                                                                 \
      nullptr,                                                                 \
      nullptr,                                                                 \
  }

#define DEFINE_REGISTER_STUB(dwarf_num, str_name)                              \
  DEFINE_GENERIC_REGISTER_STUB(dwarf_num, str_name, LLDB_INVALID_REGNUM)

using namespace lldb;
using namespace lldb_private;

LLDB_PLUGIN_DEFINE_ADV(ABIEZH, ABIEZH)

namespace {
namespace dwarf {
enum regnums {
  r0,
  r1,
  r2,
  r3,
  r4,
  r5,
  r6,
  r7,
  gpo,
  gpd,
  cfs,
  cfm,
  sp,
  pc,
  gpi,
  ra,
  flags
};

static const std::array<RegisterInfo, 17> g_register_infos = {
    {DEFINE_GENERIC_REGISTER_STUB(r0, nullptr, LLDB_REGNUM_GENERIC_ARG1),
     DEFINE_GENERIC_REGISTER_STUB(r1, nullptr, LLDB_REGNUM_GENERIC_ARG2),
     DEFINE_GENERIC_REGISTER_STUB(r2, nullptr, LLDB_REGNUM_GENERIC_ARG3),
     DEFINE_GENERIC_REGISTER_STUB(r3, nullptr, LLDB_REGNUM_GENERIC_ARG4),
     DEFINE_REGISTER_STUB(r4, nullptr), DEFINE_REGISTER_STUB(r5, nullptr),
     DEFINE_REGISTER_STUB(r6, nullptr), DEFINE_REGISTER_STUB(r7, nullptr),
     DEFINE_REGISTER_STUB(gpo, nullptr), DEFINE_REGISTER_STUB(gpd, nullptr),
     DEFINE_REGISTER_STUB(cfs, nullptr), DEFINE_REGISTER_STUB(cfm, nullptr),
     DEFINE_GENERIC_REGISTER_STUB(sp, nullptr, LLDB_REGNUM_GENERIC_SP),
     DEFINE_GENERIC_REGISTER_STUB(pc, nullptr, LLDB_REGNUM_GENERIC_PC),
     DEFINE_REGISTER_STUB(gpi, nullptr),
     DEFINE_GENERIC_REGISTER_STUB(ra, nullptr, LLDB_REGNUM_GENERIC_RA),
     DEFINE_REGISTER_STUB(flags, nullptr)}};
} // namespace dwarf
} // namespace

const RegisterInfo *ABIEZH::GetRegisterInfoArray(uint32_t &count) {
  count = dwarf::g_register_infos.size();
  return dwarf::g_register_infos.data();
}

ABISP ABIEZH::CreateInstance(ProcessSP process_sp, const ArchSpec &arch) {
  if (arch.GetTriple().getArchName().starts_with("ezh")) {
    return ABISP(new ABIEZH(std::move(process_sp), MakeMCRegisterInfo(arch)));
  }
  return ABISP();
}

UnwindPlanSP ABIEZH::CreateFunctionEntryUnwindPlan() {
  UnwindPlan::Row row;
  // CFA is SP value
  row.GetCFAValue().SetIsRegisterPlusOffset(dwarf::sp, 0);
  // PC is in RA at function entry
  row.SetRegisterLocationToRegister(dwarf::pc, dwarf::ra, true);
  row.SetRegisterLocationToRegister(dwarf::ra, dwarf::ra, true);
  for (int reg = dwarf::r0; reg <= dwarf::flags; ++reg) {
    if (reg != dwarf::pc && reg != dwarf::ra)
      row.SetRegisterLocationToSame(reg, false);
  }
  row.SetUnspecifiedRegistersAreUndefined(true);

  auto plan_sp = std::make_shared<UnwindPlan>(eRegisterKindDWARF);
  plan_sp->AppendRow(std::move(row));
  plan_sp->SetSourceName("ezh at-func-entry default");
  plan_sp->SetSourcedFromCompiler(eLazyBoolNo);
  return plan_sp;
}

UnwindPlanSP ABIEZH::CreateDefaultUnwindPlan() {
  UnwindPlan::Row row;
  // CFA is SP value
  row.GetCFAValue().SetIsRegisterPlusOffset(dwarf::sp, 0);
  // PC is in RA by default
  row.SetRegisterLocationToRegister(dwarf::pc, dwarf::ra, true);
  row.SetRegisterLocationToRegister(dwarf::ra, dwarf::ra, true);
  for (int reg = dwarf::r0; reg <= dwarf::flags; ++reg) {
    if (reg != dwarf::pc && reg != dwarf::ra)
      row.SetRegisterLocationToSame(reg, false);
  }
  row.SetUnspecifiedRegistersAreUndefined(true);

  auto plan_sp = std::make_shared<UnwindPlan>(eRegisterKindDWARF);
  plan_sp->AppendRow(std::move(row));
  plan_sp->SetSourceName("ezh default unwind plan");
  plan_sp->SetSourcedFromCompiler(eLazyBoolNo);
  return plan_sp;
}

bool ABIEZH::PrepareTrivialCall(Thread &thread, addr_t sp, addr_t function_addr,
                                addr_t return_addr,
                                llvm::ArrayRef<addr_t> args) const {
  RegisterContext *reg_ctx = thread.GetRegisterContext().get();
  if (!reg_ctx)
    return false;

  const uint32_t pc_reg = reg_ctx->ConvertRegisterKindToRegisterNumber(
      eRegisterKindGeneric, LLDB_REGNUM_GENERIC_PC);
  const uint32_t sp_reg = reg_ctx->ConvertRegisterKindToRegisterNumber(
      eRegisterKindGeneric, LLDB_REGNUM_GENERIC_SP);
  const uint32_t ra_reg = reg_ctx->ConvertRegisterKindToRegisterNumber(
      eRegisterKindGeneric, LLDB_REGNUM_GENERIC_RA);

  RegisterValue reg_value;
  const uint8_t reg_names[] = {
      LLDB_REGNUM_GENERIC_ARG1, LLDB_REGNUM_GENERIC_ARG2,
      LLDB_REGNUM_GENERIC_ARG3, LLDB_REGNUM_GENERIC_ARG4};

  llvm::ArrayRef<addr_t>::iterator ai = args.begin(), ae = args.end();

  for (size_t i = 0; i < std::size(reg_names); ++i) {
    if (ai == ae)
      break;
    reg_value.SetUInt32(*ai);
    if (!reg_ctx->WriteRegister(
            reg_ctx->GetRegisterInfo(eRegisterKindGeneric, reg_names[i]),
            reg_value))
      return false;
    ++ai;
  }

  if (ai != ae) {
    size_t num_stack_regs = ae - ai;
    sp -= (num_stack_regs * 4);
    sp &= ~(8ull - 1ull);

    const RegisterInfo *reg_info = reg_ctx->GetRegisterInfo(
        eRegisterKindGeneric, LLDB_REGNUM_GENERIC_ARG1);
    addr_t arg_pos = sp;
    for (; ai != ae; ++ai) {
      reg_value.SetUInt32(*ai);
      if (reg_ctx
              ->WriteRegisterValueToMemory(reg_info, arg_pos,
                                           reg_info->byte_size, reg_value)
              .Fail())
        return false;
      arg_pos += reg_info->byte_size;
    }
  }

  if (!reg_ctx->WriteRegisterFromUnsigned(ra_reg, return_addr))
    return false;
  if (!reg_ctx->WriteRegisterFromUnsigned(sp_reg, sp))
    return false;
  if (!reg_ctx->WriteRegisterFromUnsigned(pc_reg, function_addr))
    return false;

  return true;
}

bool ABIEZH::PrepareTrivialCall(Thread &thread, addr_t sp, addr_t pc, addr_t ra,
                                llvm::Type &prototype,
                                llvm::ArrayRef<ABI::CallArgument> args) const {
  auto reg_ctx = thread.GetRegisterContext();
  if (!reg_ctx)
    return false;

  uint32_t pc_reg = reg_ctx->ConvertRegisterKindToRegisterNumber(
      eRegisterKindGeneric, LLDB_REGNUM_GENERIC_PC);
  uint32_t ra_reg = reg_ctx->ConvertRegisterKindToRegisterNumber(
      eRegisterKindGeneric, LLDB_REGNUM_GENERIC_RA);
  uint32_t sp_reg = reg_ctx->ConvertRegisterKindToRegisterNumber(
      eRegisterKindGeneric, LLDB_REGNUM_GENERIC_SP);
  if (pc_reg == LLDB_INVALID_REGNUM || ra_reg == LLDB_INVALID_REGNUM ||
      sp_reg == LLDB_INVALID_REGNUM)
    return false;

  Status error;
  ProcessSP process = thread.GetProcess();
  if (!process)
    return false;

  for (const auto &arg : args) {
    if (arg.type == ABI::CallArgument::TargetValue)
      continue;
    sp -= arg.size;
    sp &= ~(4ull - 1ull);
    if (process->WriteMemory(sp, arg.data_up.get(), arg.size, error) <
            arg.size ||
        error.Fail())
      return false;
    *const_cast<addr_t *>(&arg.value) = sp;
  }

  size_t reg_index = LLDB_REGNUM_GENERIC_ARG1;
  for (const auto &arg : args) {
    if (reg_index <= LLDB_REGNUM_GENERIC_ARG4) {
      if (!reg_ctx->WriteRegisterFromUnsigned(
              reg_ctx->GetRegisterInfo(eRegisterKindGeneric, reg_index),
              arg.value))
        return false;
      ++reg_index;
    } else {
      sp -= 4;
      sp &= ~(4ull - 1ull);
      if (process->WriteMemory(sp, &arg.value, 4, error) < 4 || error.Fail())
        return false;
    }
  }

  reg_ctx->WriteRegisterFromUnsigned(pc_reg, pc);
  reg_ctx->WriteRegisterFromUnsigned(ra_reg, ra);
  reg_ctx->WriteRegisterFromUnsigned(sp_reg, sp);

  return true;
}

bool ABIEZH::GetArgumentValues(Thread &thread, ValueList &values) const {
  return false;
}

Status ABIEZH::SetReturnValueObject(StackFrameSP &frame_sp,
                                    ValueObjectSP &new_value_sp) {
  Status result;
  if (!new_value_sp)
    return Status::FromErrorString("Empty value object for return value.");

  CompilerType compiler_type = new_value_sp->GetCompilerType();
  if (!compiler_type)
    return Status::FromErrorString("Null clang type for return value.");

  auto &reg_ctx = *frame_sp->GetThread()->GetRegisterContext();
  DataExtractor data;
  size_t num_bytes = new_value_sp->GetData(data, result);
  if (result.Fail())
    return result;

  if (num_bytes <= 4) {
    offset_t offset = 0;
    uint64_t raw_value = data.GetMaxU64(&offset, num_bytes);
    auto reg_info =
        reg_ctx.GetRegisterInfo(eRegisterKindGeneric, LLDB_REGNUM_GENERIC_ARG1);
    if (!reg_ctx.WriteRegisterFromUnsigned(reg_info, raw_value))
      result = Status::FromErrorString("Couldn't write value to r0");
    return result;
  }
  return Status::FromErrorString(
      "We don't support returning large integer values at present.");
}

ValueObjectSP
ABIEZH::GetReturnValueObjectImpl(Thread &thread,
                                 CompilerType &compiler_type) const {
  if (!compiler_type)
    return ValueObjectSP();

  Value value;
  value.SetCompilerType(compiler_type);

  RegisterContext *reg_ctx = thread.GetRegisterContext().get();
  if (!reg_ctx)
    return ValueObjectSP();

  bool is_signed = false;
  if (compiler_type.IsIntegerOrEnumerationType(is_signed) ||
      compiler_type.IsPointerType()) {
    const RegisterInfo *r0_info = reg_ctx->GetRegisterInfo(
        eRegisterKindGeneric, LLDB_REGNUM_GENERIC_ARG1);
    uint64_t raw_value =
        reg_ctx->ReadRegisterAsUnsigned(r0_info, 0) & UINT32_MAX;

    uint64_t byte_size =
        llvm::expectedToOptional(compiler_type.GetByteSize(&thread))
            .value_or(0);
    if (byte_size == 8) {
      const RegisterInfo *r1_info = reg_ctx->GetRegisterInfo(
          eRegisterKindGeneric, LLDB_REGNUM_GENERIC_ARG2);
      raw_value |= (reg_ctx->ReadRegisterAsUnsigned(r1_info, 0) & UINT64_MAX)
                   << 32U;
    }

    value.GetScalar() = raw_value;
    if (byte_size <= 8) {
      switch (byte_size) {
      case 1:
        if (is_signed)
          value.GetScalar() = (int8_t)raw_value;
        else
          value.GetScalar() = (uint8_t)raw_value;
        break;
      case 2:
        if (is_signed)
          value.GetScalar() = (int16_t)raw_value;
        else
          value.GetScalar() = (uint16_t)raw_value;
        break;
      case 4:
        if (is_signed)
          value.GetScalar() = (int32_t)raw_value;
        else
          value.GetScalar() = (uint32_t)raw_value;
        break;
      }
    }
    return ValueObjectConstResult::Create(thread.GetStackFrameAtIndex(0).get(),
                                          value, ConstString(""));
  }
  return ValueObjectSP();
}

ValueObjectSP ABIEZH::GetReturnValueObjectImpl(Thread &thread,
                                               llvm::Type &ir_type) const {
  RegisterContext *reg_ctx = thread.GetRegisterContext().get();
  if (!reg_ctx)
    return ValueObjectSP();

  Value value;
  if (ir_type.isVoidTy()) {
    value.GetScalar() = 0;
  } else if (ir_type.isIntegerTy() || ir_type.isPointerTy()) {
    const RegisterInfo *r0_info = reg_ctx->GetRegisterInfo(
        eRegisterKindGeneric, LLDB_REGNUM_GENERIC_ARG1);
    uint64_t raw_value =
        reg_ctx->ReadRegisterAsUnsigned(r0_info, 0) & UINT32_MAX;

    size_t byte_size = 4;
    if (ir_type.isIntegerTy()) {
      byte_size = ir_type.getPrimitiveSizeInBits();
      if (byte_size != 1)
        byte_size /= CHAR_BIT;
    }

    if (byte_size == 8) {
      const RegisterInfo *r1_info = reg_ctx->GetRegisterInfo(
          eRegisterKindGeneric, LLDB_REGNUM_GENERIC_ARG2);
      raw_value |= (reg_ctx->ReadRegisterAsUnsigned(r1_info, 0) & UINT64_MAX)
                   << 32U;
    }
    value.GetScalar() = raw_value;
  } else {
    return ValueObjectSP();
  }

  return ValueObjectConstResult::Create(thread.GetStackFrameAtIndex(0).get(),
                                        value, ConstString(""));
}

bool ABIEZH::RegisterIsVolatile(const RegisterInfo *reg_info) {
  if (!reg_info)
    return false;
  uint32_t regnum = reg_info->kinds[eRegisterKindDWARF];
  if (regnum <= dwarf::r3 || regnum == dwarf::ra)
    return true;
  return false;
}

void ABIEZH::Initialize() {
  PluginManager::RegisterPlugin(GetPluginNameStatic(),
                                "System ABI for EZH targets", CreateInstance);
}

void ABIEZH::Terminate() { PluginManager::UnregisterPlugin(CreateInstance); }
