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
  // This pin is CORRECTNESS-CRITICAL, not a performance choice. The
  // condition flags are not modelled as a register, so the only thing
  // keeping a flag-setting (S-form) producer adjacent to its predicated
  // consumer is that no instruction-motion pass runs after the if-converter
  // creates predicated instances. Descriptor flags do NOT protect this:
  // while the immediate materializers, GOTO/TCRETURN, and the EZHInstALU
  // formats carry hasSideEffects=1, tablegen infers PURE descriptors from
  // the ISel patterns of the shift, bit-op, ANDOR, and memory formats -- 254
  // predicable descriptors have no side-effect flag. A reachable example
  // after if-conversion is "SUB_IMM_s ...; LSL ..., 2, 1": the predicated
  // LSL reads flags but its descriptor gives a scheduler no dependence on
  // the producer, and marking only the producer side-effecting creates no
  // edge to a pure consumer. (An earlier note here claimed the pin was
  // droppable; that conclusion came from observing a side-effecting
  // consumer, mov_cc, and was wrong for the pure-descriptor forms.)
  //
  // Dropping the pin therefore first requires closing the modeling gap:
  // either model the flags as an implicit physical register (ARM-CPSR
  // style), or give every reachable predicated-consumer and flag-writer
  // form an honest descriptor (_CC-twin splits or hasSideEffects on the
  // remaining formats). See ezh/OPT_BACKLOG.md for the full analysis.
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
