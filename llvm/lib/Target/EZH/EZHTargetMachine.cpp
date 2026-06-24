//===-- EZHTargetMachine.cpp - Define TargetMachine for EZH ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the info about EZH target spec.
//
//===----------------------------------------------------------------------===//

#include "EZHTargetMachine.h"

#include "EZH.h"
#include "EZHMachineFunctionInfo.h"
#include "EZHTargetObjectFile.h"
#include "EZHTargetTransformInfo.h"
#include "TargetInfo/EZHTargetInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Target/TargetOptions.h"
#include <optional>

using namespace llvm;

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeEZHTarget() {
  // Register the target.
  RegisterTargetMachine<EZHTargetMachine> registered_target(getTheEZHTarget());
  PassRegistry &PR = *PassRegistry::getPassRegistry();
  initializeEZHAsmPrinterPass(PR);
  initializeEZHDAGToDAGISelLegacyPass(PR);
  initializeEZHMemAluCombinerPass(PR);
}

static Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
  return RM.value_or(Reloc::PIC_);
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
      TLOF(new EZHTargetObjectFile()) {
  initAsmInfo();
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
// EZH Code Generator Pass Configuration Options.
class EZHPassConfig : public TargetPassConfig {
public:
  EZHPassConfig(EZHTargetMachine &TM, PassManagerBase *PassManager)
      : TargetPassConfig(TM, *PassManager) {}

  EZHTargetMachine &getEZHTargetMachine() const {
    return getTM<EZHTargetMachine>();
  }

  void addIRPasses() override;
  bool addInstSelector() override;
  void addPreSched2() override;
  void addPreEmitPass() override;
};
} // namespace

TargetPassConfig *
EZHTargetMachine::createPassConfig(PassManagerBase &PassManager) {
  return new EZHPassConfig(*this, &PassManager);
}

void EZHPassConfig::addIRPasses() {
  addPass(createAtomicExpandLegacyPass());

  TargetPassConfig::addIRPasses();
}

// Install an instruction selector pass.
bool EZHPassConfig::addInstSelector() {
  addPass(createEZHISelDag(getEZHTargetMachine()));
  return false;
}

// Implemented by targets that want to run passes immediately before
// machine code is emitted.
void EZHPassConfig::addPreEmitPass() {
  addPass(createEZHDelaySlotFillerPass(getEZHTargetMachine()));
}

// Run passes after prolog-epilog insertion and before the second instruction
// scheduling pass.
void EZHPassConfig::addPreSched2() { addPass(createEZHMemAluCombinerPass()); }
