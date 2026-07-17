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
//   Renamed LanaiTargetMachine to EZHTargetMachine; updated target layout
//   string (e-m:e-p:32:32-i8:8:32-i16:16:32-i64:64-n32), custom pass pipeline
//   creation, and subtarget ownership.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EZH_EZHTARGETMACHINE_H
#define LLVM_LIB_TARGET_EZH_EZHTARGETMACHINE_H

#include "EZHISelLowering.h"
#include "EZHInstrInfo.h"
#include "EZHSelectionDAGInfo.h"
#include "EZHSubtarget.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Target/TargetLoweringObjectFile.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Triple.h"
#include <memory>
#include <optional>

namespace llvm {

// PassManagerBase comes from llvm/Target/TargetMachine.h (as `using
// legacy::PassManagerBase`); a local forward declaration here conflicts with
// that on current LLVM and makes createPassConfig's override ambiguous.

/// TargetMachine implementation for the NXP EZH architecture.
class EZHTargetMachine : public CodeGenTargetMachineImpl {
  EZHSubtarget Subtarget;
  std::unique_ptr<TargetLoweringObjectFile> TLOF;

public:
  EZHTargetMachine(const Target &TheTarget, const Triple &TargetTriple,
                   StringRef Cpu, StringRef FeatureString,
                   const TargetOptions &Options, std::optional<Reloc::Model> RM,
                   std::optional<CodeModel::Model> CodeModel,
                   CodeGenOptLevel OptLevel, bool JIT);

  const EZHSubtarget *getSubtargetImpl(const Function & /*Fn*/) const override {
    return &Subtarget;
  }

  TargetTransformInfo getTargetTransformInfo(const Function &F) const override;

  // Pass Pipeline Configuration
  TargetPassConfig *createPassConfig(PassManagerBase &pass_manager) override;

  // Claim responsibility for post-RA scheduling and then deliberately
  // schedule nothing: this is the hook that keeps the generic pipeline
  // from ever inserting a post-RA scheduler, including through the
  // -post-RA-scheduler and -misched-postra options (a subtarget
  // enablePostRAScheduler override is bypassable by both).
  //
  // History: this pin used to be CORRECTNESS-CRITICAL -- the condition flags
  // were invisible to the machine layer, so hundreds of predicable pure
  // descriptors (shift/bit-op/ANDOR/memory formats; tablegen infers purity
  // from their ISel patterns) had no modeled dependence on their S-form flag
  // producers, and only pass ordering kept "SUB_IMM_s; LSL ..., 2, 1"-style
  // pairs adjacent. The flags are NOW modeled as the reserved CFS register
  // (S-forms Defs=[CFS]; predicated instances and carry readers use it), so
  // every such pair carries a true register dependence. The pin is retained
  // as defense-in-depth and because scheduling is pure churn under
  // NoSchedModel; dropping it is worthwhile only together with a real
  // SchedModel to hide the load-use latency -- a separate, silicon-validated
  // effort. See ezh/OPT_BACKLOG.md.
  bool targetSchedulesPostRAScheduling() const override { return true; }

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
