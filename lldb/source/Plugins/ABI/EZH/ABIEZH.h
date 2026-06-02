//===-- ABIEZH.h ------------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_ABIEZH_h_
#define liblldb_ABIEZH_h_

#include "lldb/Target/ABI.h"
#include "lldb/lldb-private.h"

class ABIEZH : public lldb_private::RegInfoBasedABI {
public:
  ~ABIEZH() override = default;

  size_t GetRedZoneSize() const override { return 0; }

  bool PrepareTrivialCall(lldb_private::Thread &thread, lldb::addr_t sp,
                          lldb::addr_t functionAddress,
                          lldb::addr_t returnAddress,
                          llvm::ArrayRef<lldb::addr_t> args) const override;

  bool
  PrepareTrivialCall(lldb_private::Thread &thread, lldb::addr_t sp,
                     lldb::addr_t functionAddress, lldb::addr_t returnAddress,
                     llvm::Type &prototype,
                     llvm::ArrayRef<ABI::CallArgument> args) const override;

  bool GetArgumentValues(lldb_private::Thread &thread,
                         lldb_private::ValueList &values) const override;

  lldb_private::Status
  SetReturnValueObject(lldb::StackFrameSP &frame_sp,
                       lldb::ValueObjectSP &new_value) override;

  lldb::ValueObjectSP
  GetReturnValueObjectImpl(lldb_private::Thread &thread,
                           lldb_private::CompilerType &type) const override;
  lldb::ValueObjectSP GetReturnValueObjectImpl(lldb_private::Thread &thread,
                                               llvm::Type &type) const override;

  lldb::UnwindPlanSP CreateFunctionEntryUnwindPlan() override;

  lldb::UnwindPlanSP CreateDefaultUnwindPlan() override;

  bool RegisterIsVolatile(const lldb_private::RegisterInfo *reg_info) override;

  bool CallFrameAddressIsValid(lldb::addr_t cfa) override {
    return (cfa & 0x3ull) == 0; // 4-byte aligned stack
  }

  bool CodeAddressIsValid(lldb::addr_t pc) override {
    return (pc & 0x3ull) == 0; // EZH instructions are 4-byte aligned
  }

  const lldb_private::RegisterInfo *
  GetRegisterInfoArray(uint32_t &count) override;

  static void Initialize();

  static void Terminate();

  static lldb::ABISP CreateInstance(lldb::ProcessSP process_sp,
                                    const lldb_private::ArchSpec &arch);

  static llvm::StringRef GetPluginNameStatic() { return "ezh"; }

  llvm::StringRef GetPluginName() override { return GetPluginNameStatic(); }

private:
  using lldb_private::RegInfoBasedABI::RegInfoBasedABI;
};

#endif // liblldb_ABIEZH_h_
