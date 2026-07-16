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

        // A predicated (conditional) tail call must never reach this pass in
        // bitslice mode: it keeps RA live across a fall-through, so RA has no
        // safe stack slot for the poll to hide in. The codegen invariant is
        // that bitslice mode force-saves RA (determineCalleeSaves) and lowers a
        // conditional tail call to popd_cc pc + an unconditional goto instead,
        // so only the unconditional TCRETURN* forms (handled below) appear here.
        // Assert it, so a future change that breaks the invariant trips loudly
        // rather than silently injecting a gotol_bs that clobbers the live RA.
        assert(MI.getOpcode() != EZH::TCRETURN_CC &&
               MI.getOpcode() != EZH::TCRETURNExt_CC &&
               MI.getOpcode() != EZH::TCRETURN_REG_CC &&
               MI.getOpcode() != EZH::TCRETURN_MEM_CC &&
               "predicated tail call in a bitslice-interrupt function");

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
          // A block-ending run of terminators (a conditional GOTO_CC then
          // the fall-through GOTO) needs only one poll, before the first of
          // them; injecting before a later branch would wedge the poll (a
          // non-terminator conditional call) between two terminators. So
          // skip a branch whose predecessor is already a terminator.
          if (MI.isBranch() && MI.getIterator() != MBB.begin() &&
              std::prev(MI.getIterator())->isTerminator())
            continue;
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
