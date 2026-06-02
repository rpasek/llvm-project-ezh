//===-- EZHTargetMachine.h - Define TargetMachine for EZH --- C++ ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the EZH specific subclass of TargetMachine.
//
// Description:
//   Declares EZHTargetMachine, the primary target machine implementation for
//   the EZH (SmartDMA) architecture.
//
// Copied From:
//   Lanai target backend (llvm/lib/Target/Lanai/LanaiTargetMachine.h).
//
// Changes:
//   Renamed LanaiTargetMachine to EZHTargetMachine
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EZH_EZHTARGETMACHINE_H
#define LLVM_LIB_TARGET_EZH_EZHTARGETMACHINE_H

#include "EZHISelLowering.h"
#include "EZHInstrInfo.h"
#include "EZHSelectionDAGInfo.h"
#include "EZHSubtarget.h"
#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include <optional>

namespace llvm {

class EZHTargetMachine : public CodeGenTargetMachineImpl {
  EZHSubtarget Subtarget;
  std::unique_ptr<TargetLoweringObjectFile> TLOF;

public:
  EZHTargetMachine(const Target &TheTarget, const Triple &TargetTriple,
                   StringRef Cpu, StringRef FeatureString,
                   const TargetOptions &Options, std::optional<Reloc::Model> RM,
                   std::optional<CodeModel::Model> CodeModel,
                   CodeGenOptLevel OptLevel, bool JIT);

  const EZHSubtarget *
  getSubtargetImpl(const llvm::Function & /*Fn*/) const override {
    return &Subtarget;
  }

  TargetTransformInfo getTargetTransformInfo(const Function &F) const override;

  // Pass Pipeline Configuration
  TargetPassConfig *createPassConfig(PassManagerBase &pass_manager) override;

  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }

  MachineFunctionInfo *
  createMachineFunctionInfo(BumpPtrAllocator &Allocator, const Function &F,
                            const TargetSubtargetInfo *STI) const override;

  bool isMachineVerifierClean() const override { return false; }
};
} // namespace llvm

#endif // LLVM_LIB_TARGET_EZH_EZHTARGETMACHINE_H
