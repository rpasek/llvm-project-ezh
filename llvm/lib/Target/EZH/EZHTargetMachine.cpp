//===-- EZHTargetMachine.cpp - Define TargetMachine for EZH ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Description:
//   Implements target machine setup, subtarget creation, and the EZH CodeGen
//   pass pipeline.
//
// Copied From:
//   Lanai target backend (llvm/lib/Target/Lanai/LanaiTargetMachine.cpp).
//
// Changes:
//   Configured custom pass pipeline adding EZHBitSliceInjection
//   and EZHConstantIslandPass; set default relocation model
//   and optimization levels for EZH.
//
//===----------------------------------------------------------------------===//

#include "EZHTargetMachine.h"
#include "EZH.h"
#include "EZHMachineFunctionInfo.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "TargetInfo/EZHTargetInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/MC/TargetRegistry.h"
#include <optional>

using namespace llvm;

namespace {
class EZHTTIImpl : public BasicTTIImplBase<EZHTTIImpl> {
  using BaseT = BasicTTIImplBase<EZHTTIImpl>;
  using TTI = TargetTransformInfo;
  friend BaseT;
  friend TargetTransformInfoImplBase;

  const EZHSubtarget *ST;
  const EZHTargetLowering *TLI;

  const EZHSubtarget *getST() const { return ST; }
  const EZHTargetLowering *getTLI() const { return TLI; }

public:
  explicit EZHTTIImpl(const EZHTargetMachine *TM, const Function &F)
      : BaseT(TM, F.getDataLayout()), ST(TM->getSubtargetImpl(F)),
        TLI(ST->getTargetLowering()) {}

  // Bias loop strength reduction toward post-increment addressing: the
  // ldr_post/str_post forms fold the pointer bump into the access, which
  // matters most in the byte-pump loops this core exists for. Without the
  // preference LSR happily shares one induction variable between the
  // address and a counter, paying a reg-offset access plus a separate
  // increment every iteration.
  TTI::AddressingModeKind
  getPreferredAddressingMode(const Loop *L,
                             ScalarEvolution *SE) const override {
    return TTI::AMK_PostIndexed;
  }
};
} // namespace

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEZHTarget() {
  RegisterTargetMachine<EZHTargetMachine> registered_target(getTheEZHTarget());
}

static Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
  return Reloc::Static;
}

EZHTargetMachine::EZHTargetMachine(const Target &T, const Triple &TT,
                                   StringRef Cpu, StringRef FeatureString,
                                   const TargetOptions &Options,
                                   std::optional<Reloc::Model> RM,
                                   std::optional<CodeModel::Model> CodeModel,
                                   CodeGenOptLevel OptLevel, bool JIT)
    : CodeGenTargetMachineImpl(
          T, TT.computeDataLayout(), TT, Cpu, FeatureString, Options,
          getEffectiveRelocModel(RM),
          getEffectiveCodeModel(CodeModel, CodeModel::Medium), OptLevel),
      Subtarget(TT, Cpu, FeatureString, *this, Options, getCodeModel(),
                OptLevel),
      TLOF(std::make_unique<TargetLoweringObjectFileELF>()) {
  initAsmInfo();
  // Opt into the target-default MachineOutliner: enable the pass and let it
  // run by default when the function requests minimum size. The generic pass
  // runs before addPreEmitPass2, so it never sees injected gotol_bs polls or
  // materialized pc-relative island loads. EZHInstrInfo's whitelist-closed
  // classification governs what may be outlined.
  setMachineOutliner(true);
  setSupportsDefaultOutlining(true);
}

TargetTransformInfo
EZHTargetMachine::getTargetTransformInfo(const Function &F) const {
  return TargetTransformInfo(std::make_unique<EZHTTIImpl>(this, F));
}

MachineFunctionInfo *EZHTargetMachine::createMachineFunctionInfo(
    BumpPtrAllocator &Allocator, const Function &F,
    const TargetSubtargetInfo *STI) const {
  return EZHMachineFunctionInfo::create<EZHMachineFunctionInfo>(Allocator, F,
                                                                STI);
}

namespace {
class EZHPassConfig : public TargetPassConfig {
public:
  EZHPassConfig(EZHTargetMachine &TM, PassManagerBase *PassManager)
      : TargetPassConfig(TM, *PassManager) {}

  EZHTargetMachine &getEZHTargetMachine() const {
    return getTM<EZHTargetMachine>();
  }

  void addIRPasses() override;
  bool addPreISel() override;
  bool addInstSelector() override;
  void addPostRegAlloc() override;
  void addPreSched2() override;
  void addPreEmitPass() override;
  void addPreEmitPass2() override;
};
} // namespace

TargetPassConfig *
EZHTargetMachine::createPassConfig(PassManagerBase &PassManager) {
  return new EZHPassConfig(*this, &PassManager);
}

void EZHPassConfig::addIRPasses() { TargetPassConfig::addIRPasses(); }

bool EZHPassConfig::addPreISel() {
  // Merge small globals into one blob: every global otherwise costs its
  // own constant-pool entry plus a pc-relative load per function that
  // touches it, while a merged blob shares a single pooled base address
  // whose member offsets fold straight into the load/store immediates.
  // 508 is the word-offset addressing limit; byte accesses past 127 pay
  // one add_imm but still save the pool slot and load. Placed in
  // addPreISel like the other targets running GlobalMerge, after the
  // generic IR pipeline and the input verifier.
  if (TM->getOptLevel() != CodeGenOptLevel::None)
    addPass(createGlobalMergePass(TM, 508, /*OnlyOptimizeForSize=*/false,
                                  /*MergeExternalByDefault=*/true));
  return false;
}

bool EZHPassConfig::addInstSelector() {
  addPass(createEZHISelDag(getEZHTargetMachine()));
  return false;
}

void EZHPassConfig::addPostRegAlloc() {}

void EZHPassConfig::addPreSched2() {
  if (getOptLevel() != CodeGenOptLevel::None)
    addPass(&IfConverterID);
}

void EZHPassConfig::addPreEmitPass() {
  if (getOptLevel() != CodeGenOptLevel::None) {
    addPass(createEZHCompareFusionPass());
    // Post-RA list scheduling, placed AFTER compare fusion so fusion sees the
    // unscheduled adjacent pairs, and after the if-converter so predicated
    // instances exist -- their implicit CFS operands (flags-as-physreg model)
    // carry the dependences that make reordering here sound. The generic
    // pipeline's insertion point is suppressed by
    // targetSchedulesPostRAScheduling; this is the target-owned placement.
    addPass(&PostRASchedulerID);
  }
}

void EZHPassConfig::addPreEmitPass2() {
  // Hardware-loop formation runs after the post-RA scheduler (the repeated
  // block keeps its final schedule) and before the bitslice injection and
  // constant-island passes; it self-gates on optlevel, optsize, and the
  // bitslice-interrupts feature.
  if (getOptLevel() != CodeGenOptLevel::None)
    addPass(createEZHTightLoopFormationPass());
  addPass(createEZHBitSliceInjectionPass());
  addPass(createEZHConstantIslandPass());
}
