//===-- EZHBitSliceInjection.cpp - EZH BitSlice Interrupt Workaround ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Description:
//   This pass implements a workaround for EZH core's lack of interrupts.
//   It injects a conditional 'gotol_bs bitslice_handler' instruction before
//   every direct branch or direct call instruction if the bitslice-interrupts
//   subtarget feature is enabled.
//
// Copied From:
//   Newly authored custom file for the EZH (SmartDMA) target architecture
//
// Changes:
//   Authored from scratch to pattern-match IR/MachineInstr bit manipulation
//   sequences into specialized EZH instructions defined in fsl_smartdma_prv.h.
//
//===----------------------------------------------------------------------===//

#include "EZH.h"
#include "EZHCondCode.h"
#include "EZHInstrInfo.h"
#include "EZHSubtarget.h"
#include "MCTargetDesc/EZHMCTargetDesc.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

#define DEBUG_TYPE "ezh-bitslice-injection"

namespace {
class EZHBitSliceInjection : public MachineFunctionPass {
public:
  inline static char ID = 0;
  EZHBitSliceInjection() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    const EZHSubtarget &ST = MF.getSubtarget<EZHSubtarget>();
    if (!ST.hasBitSliceInterrupts())
      return false;

    // Do not recursively inject into the bitslice handler itself!
    if (MF.getName() == "bitslice_handler")
      return false;

    const TargetInstrInfo *TII = ST.getInstrInfo();
    bool Changed = false;

    for (auto &MBB : MF) {
      for (auto I = MBB.begin(), E = MBB.end(); I != E;) {
        MachineInstr &MI = *I;
        ++I; // Increment iterator early as we might insert before MI

        bool IsBranchOrCall = MI.isBranch() || MI.isCall();

        // A tail call still needs its poll -- a cycle of unconditional
        // tail calls would otherwise never enter the handler -- but the
        // injection cannot go directly before it: the epilogue has already
        // restored the live return address into RA and gotol_bs writes RA.
        // Instead, inject before the epilogue (the contiguous FrameDestroy
        // run ending at the tail call), where RA's value still sits safely
        // in its stack slot. determineCalleeSaves guarantees that a
        // bitslice-mode function containing a tail call saves RA, so that
        // slot always exists.
        if (MI.getOpcode() == EZH::TCRETURN ||
            MI.getOpcode() == EZH::TCRETURNExt ||
            MI.getOpcode() == EZH::TCRETURN_REG ||
            MI.getOpcode() == EZH::TCRETURN_MEM) {
          MachineBasicBlock::iterator InsertPt = MI.getIterator();
          while (InsertPt != MBB.begin() &&
                 std::prev(InsertPt)->getFlag(MachineInstr::FrameDestroy))
            --InsertPt;
          BuildMI(MBB, InsertPt, MI.getDebugLoc(), TII->get(EZH::GOTOL))
              .addExternalSymbol("bitslice_handler")
              .addImm(EZHCC::ICC_BS);
          Changed = true;
          continue;
        }

        if (IsBranchOrCall) {
          BuildMI(MBB, MI, MI.getDebugLoc(), TII->get(EZH::GOTOL))
              .addExternalSymbol("bitslice_handler")
              .addImm(EZHCC::ICC_BS);
          Changed = true;
        }
      }
    }

    return Changed;
  }
};
} // namespace

namespace llvm {
FunctionPass *createEZHBitSliceInjectionPass() {
  return new EZHBitSliceInjection();
}
} // namespace llvm
