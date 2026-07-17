//===-- EZHTightLoopFormation.cpp - Form tight_loop hardware loops -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Description:
//   Convert canonical counted machine loops into the EZH tight_loop
//   zero-overhead hardware loop.
//
//   The EZH tight_loop instruction (AN14650 E_TIGHT_LOOP, silicon-validated
//   semantics) repeats a straight-line block automatically:
//
//     tight_loop Rend, Rcount   ; Rend = address AFTER the last repeated
//                               ;        instruction (latched at entry)
//     <slot>                    ; executed exactly ONCE (not part of the loop)
//     <block>                   ; repeated Rcount+1 times
//   Rend:
//
//   Down-counted loops canonicalize on EZH to a single-block form:
//
//     guard:  sub_imms n, n, 0 ; goto_zb exit
//     body:   ...body...
//             add_imms cnt, cnt, -1
//             goto_nz body
//     exit:                     (layout successor)
//
//   which this pass rewrites (when the constraints below hold) to:
//
//     body:   load_constant rEnd, <cpi of exit label>
//             add_imm cnt, cnt, -1        ; Rcount = n-1
//             tight_loop rEnd, cnt
//             nop                          ; the run-once slot
//             ...body...                   ; repeated n times by hardware
//     exit:
//
//   saving the decrement + backedge branch on every iteration.
//
//   Constraints (bail out and keep the ordinary loop otherwise):
//    - innermost, single-block loop; exit is the layout successor and the
//      only exit; the sole terminator is goto_nz to the header.
//    - the branch's flags come from an add_imms cnt, cnt, -1 (or
//      sub_imms cnt, cnt, 1) with dst==src and an EU predicate, and no
//      other CFS reader sits between the decrement and the branch.
//    - the counter is not otherwise read or written in the body and is
//      dead out of the loop (after conversion it holds n-1, not 0).
//    - no instruction in the exit block reads CFS before defining it (the
//      deleted decrement's final flags must be unobservable).
//    - the body reads CFS only after defining it (iterations 2..n of the
//      original loop entered with the decrement's flags; that loop-carried
//      flag dependence cannot be preserved).
//    - no calls, branches, barriers, PC/RA/SP writes, or nested tight_loop
//      in the body; at least one repeated instruction; body small enough
//      (EZHTightLoopMaxBody) that constant-island water before the loop
//      always stays in range of the setup load.
//    - a scratch GPR for Rend is free at the head of the loop.
//
//   The pass runs at the top of addPreEmitPass2 -- after the post-RA
//   scheduler (the body keeps its final schedule) and before
//   EZHBitSliceInjection and EZHConstantIslandPass. It is disabled when
//   bitslice interrupts are enabled: the interrupt workaround polls at
//   branches, and deleting the backedge would let a long-running loop
//   starve the bitslice handler. It registers the loop block with
//   EZHMachineFunctionInfo so the constant-island pass never places an
//   island (whose data and branch-around goto would fall inside the
//   repeated region) between the body and the exit label, and never splits
//   the body block.
//
// Copied From:
//   Newly authored custom file for the EZH (SmartDMA) target architecture.
//
// Changes:
//   Authored from scratch around the AN14650 E_TIGHT_LOOP semantics
//   (run-once slot, Rend-after-body, Rcount+1 executions), validated on
//   EVK-MIMXRT595 silicon.
//
//===----------------------------------------------------------------------===//

#include "EZH.h"
#include "EZHCondCode.h"
#include "EZHConstantPoolValue.h"
#include "EZHInstrInfo.h"
#include "EZHMachineFunctionInfo.h"
#include "EZHSubtarget.h"
#include "MCTargetDesc/EZHMCTargetDesc.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

#define DEBUG_TYPE "ezh-tight-loop"

STATISTIC(NumTightLoops, "Number of tight_loop hardware loops formed");

static cl::opt<bool>
    EnableTightLoops("ezh-tight-loops", cl::Hidden, cl::init(true),
                     cl::desc("Form EZH tight_loop hardware loops"));

// Bodies larger than this are left as ordinary loops. Keeping the setup
// load_constant within a short distance of the loop head guarantees the
// constant-island pass always finds water (before the loop) in range, since
// the loop block itself is excluded from water placement.
static cl::opt<unsigned> TightLoopMaxBody(
    "ezh-tight-loop-max-body", cl::Hidden, cl::init(100),
    cl::desc("Maximum repeated-block size (instructions) for tight_loop"));

namespace {
class EZHTightLoopFormation : public MachineFunctionPass {
public:
  inline static char ID = 0;
  EZHTightLoopFormation() : MachineFunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineLoopInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return "EZH tight_loop hardware loop formation";
  }

private:
  const EZHInstrInfo *TII = nullptr;
  const TargetRegisterInfo *TRI = nullptr;

  bool tryConvertLoop(MachineLoop *L, MachineFunction &MF);
};
} // namespace

bool EZHTightLoopFormation::runOnMachineFunction(MachineFunction &MF) {
  if (!EnableTightLoops || skipFunction(MF.getFunction()))
    return false;

  const EZHSubtarget &ST = MF.getSubtarget<EZHSubtarget>();
  // The bitslice interrupt workaround polls at branches; a hardware loop has
  // no per-iteration branch, so converting would starve the handler for the
  // whole loop. Keep ordinary loops (and their backedge polls) in that mode.
  if (ST.hasBitSliceInterrupts())
    return false;

  // The conversion trades 2 instructions per iteration for 16 bytes of
  // one-time setup; at optsize/minsize keep the smaller form.
  if (MF.getFunction().hasOptSize())
    return false;

  TII = ST.getInstrInfo();
  TRI = ST.getRegisterInfo();
  MachineLoopInfo &MLI = getAnalysis<MachineLoopInfoWrapperPass>().getLI();

  bool Changed = false;
  SmallVector<MachineLoop *, 8> Loops(MLI.begin(), MLI.end());
  while (!Loops.empty()) {
    MachineLoop *L = Loops.pop_back_val();
    // Visit children first; only innermost loops are candidates, but an
    // outer loop may become convertible in a future improvement.
    Loops.append(L->begin(), L->end());
    if (L->isInnermost())
      Changed |= tryConvertLoop(L, MF);
  }
  return Changed;
}

bool EZHTightLoopFormation::tryConvertLoop(MachineLoop *L,
                                           MachineFunction &MF) {
  if (L->getNumBlocks() != 1)
    return false;
  MachineBasicBlock *B = L->getHeader();

  MachineBasicBlock *Exit = L->getExitBlock();
  if (!Exit || !B->isLayoutSuccessor(Exit))
    return false;

  // The constant-island pass may need to create water immediately BEFORE the
  // loop for the setup load (see createNewWater); require a layout
  // predecessor that is not itself a converted body, so that spot always
  // exists and never falls inside another loop's repeated region.
  if (B->getIterator() == MF.begin())
    return false;
  const MachineBasicBlock *Prev = &*std::prev(B->getIterator());
  if (MF.getInfo<EZHMachineFunctionInfo>()->isTightLoopBody(Prev))
    return false;
  // Symmetric closure regardless of loop visit order: if OUR exit block is
  // already a converted body, converting us would make that body's layout
  // predecessor a converted body -- the situation the island-pass guard
  // relies on never happening.
  if (MF.getInfo<EZHMachineFunctionInfo>()->isTightLoopBody(Exit))
    return false;

  // Sole terminator: goto_nz back to the header, exit by fall-through.
  MachineBasicBlock::iterator TI = B->getFirstTerminator();
  if (TI == B->end() || std::next(TI) != B->end())
    return false;
  if (TI->getOpcode() != EZH::GOTO_CC || !TI->getOperand(0).isMBB() ||
      TI->getOperand(0).getMBB() != B ||
      TI->getOperand(1).getImm() != EZHCC::ICC_NZ)
    return false;

  // The branch's flags must come from the canonical counter decrement, with
  // no other CFS reader in between (it would observe the deleted flags).
  MachineInstr *Dec = nullptr;
  for (MachineBasicBlock::iterator I = TI;;) {
    if (I == B->begin())
      return false;
    --I;
    if (I->isMetaInstruction())
      continue;
    if (I->definesRegister(EZH::CFS, TRI)) {
      Dec = &*I;
      break;
    }
    if (I->readsRegister(EZH::CFS, TRI))
      return false;
  }
  unsigned DecOpc = Dec->getOpcode();
  int64_t DecImm;
  if (DecOpc == EZH::ADD_IMM_s)
    DecImm = -1;
  else if (DecOpc == EZH::SUB_IMM_s)
    DecImm = 1;
  else
    return false;
  if (Dec->getNumExplicitOperands() < 4 || !Dec->getOperand(0).isReg() ||
      !Dec->getOperand(1).isReg() || !Dec->getOperand(2).isImm() ||
      Dec->getOperand(2).getImm() != DecImm ||
      Dec->getOperand(3).getImm() != EZHCC::ICC_EU)
    return false;
  Register Cnt = Dec->getOperand(0).getReg();
  if (Dec->getOperand(1).getReg() != Cnt)
    return false;

  // After conversion the counter register holds n-1 (never reaches 0), so it
  // must be dead out of the loop, and the body must not observe it.
  if (Exit->isLiveIn(Cnt))
    return false;

  // tight_loop always runs the block Rcount+1 times with Rcount = n-1
  // computed by a plain 32-bit decrement. For n >= 1 that is exactly the
  // original do-while count. For an (invalid-input) n == 0 the ORIGINAL
  // rotated loop runs 2^32 iterations; whether the hardware counter wraps
  // identically for Rcount = 0xFFFFFFFF is unverified silicon behavior --
  // so require positive evidence that n >= 1: either the canonical zero
  // guard (the unique external predecessor skips the loop to the exit when
  // the counter is zero) or a constant counter init >= 1.
  if (B->pred_size() != 2)
    return false; // exactly the backedge plus one external predecessor
  MachineBasicBlock *Pre = nullptr;
  for (MachineBasicBlock *P : B->predecessors())
    if (P != B)
      Pre = P;
  if (!Pre)
    return false;
  bool CountIsPositive = false;
  // Walk up the unique-predecessor chain from the external predecessor,
  // through blocks that leave the counter untouched, looking for either
  // form of evidence. A join block ends the walk (another path might not
  // guard the counter).
  MachineBasicBlock *P = Pre;
  for (unsigned Hops = 0; P && Hops < 4 && !CountIsPositive; ++Hops) {
    // (a) canonical guard: P ends with a conditional branch to the exit on a
    //     zero condition, fed by a compare-with-zero of the counter.
    MachineBasicBlock::iterator GT = P->getFirstTerminator();
    if (GT != P->end() && GT->getOpcode() == EZH::GOTO_CC &&
        GT->getOperand(0).isMBB() && GT->getOperand(0).getMBB() == Exit &&
        (GT->getOperand(1).getImm() == EZHCC::ICC_ZE ||
         GT->getOperand(1).getImm() == EZHCC::ICC_ZB)) {
      for (MachineBasicBlock::iterator I = GT; I != P->begin();) {
        --I;
        if (I->isMetaInstruction())
          continue;
        // The guard proves the value of Cnt AT THE COMPARE; any redefinition
        // between the compare and the branch (e.g. a phi-elimination copy)
        // means a different value enters the loop.
        if (I->definesRegister(Cnt, TRI))
          break;
        if (I->definesRegister(EZH::CFS, TRI)) {
          if (I->getOpcode() == EZH::SUB_IMM_s && I->getOperand(1).isReg() &&
              I->getOperand(1).getReg() == Cnt && I->getOperand(2).isImm() &&
              I->getOperand(2).getImm() == 0)
            CountIsPositive = true;
          break;
        }
        if (I->readsRegister(EZH::CFS, TRI))
          break;
      }
      if (CountIsPositive)
        break;
    }
    // (b) constant init: the last write of the counter in P is
    //     load_imm Cnt, K with K >= 1. Any other write ends the walk.
    bool DefinedHere = false;
    for (MachineBasicBlock::iterator I = P->end(); I != P->begin();) {
      --I;
      if (I->isMetaInstruction())
        continue;
      if (!I->definesRegister(Cnt, TRI))
        continue;
      DefinedHere = true;
      if (I->getOpcode() == EZH::LOAD_IMM && I->getOperand(1).isImm() &&
          I->getOperand(1).getImm() >= 1)
        CountIsPositive = true;
      break;
    }
    if (DefinedHere)
      break;
    P = P->pred_size() == 1 ? *P->pred_begin() : nullptr;
  }
  if (!CountIsPositive)
    return false;

  // The decrement's final flags likewise become unobservable: the exit block
  // must not read CFS before writing it, and if it neither reads nor writes
  // the flags they must DIE there (no successors) -- otherwise they could
  // flow through the CFS-neutral exit to a consumer in a later block.
  {
    bool ExitKillsFlags = false;
    for (const MachineInstr &MI : *Exit) {
      if (MI.isMetaInstruction())
        continue;
      if (MI.readsRegister(EZH::CFS, TRI))
        return false;
      if (MI.definesRegister(EZH::CFS, TRI)) {
        ExitKillsFlags = true;
        break;
      }
    }
    if (!ExitKillsFlags && !Exit->succ_empty())
      return false;
  }

  // Body legality scan (everything except the decrement and the branch).
  unsigned BodySize = 0;
  unsigned BodyPoolUsers = 0;
  bool SeenCFSDef = false;
  for (MachineInstr &MI : *B) {
    if (&MI == Dec || &MI == &*TI)
      continue;
    // Debug and other meta instructions emit nothing: they are not body,
    // and a debug USE of the counter must not veto the conversion (the
    // transformation must be -g invariant).
    if (MI.isMetaInstruction())
      continue;
    if (MI.isCall() || MI.isBranch() || MI.isTerminator() || MI.isBarrier() ||
        MI.getOpcode() == EZH::TIGHT_LOOP)
      return false;
    // Inline asm: its emitted size is unknown (an empty asm would make the
    // hardware-repeated region ZERO bytes -- destroying the classic
    // asm-volatile delay-loop idiom -- and a large one busts the size cap),
    // and its text may contain branches invisible to this scan.
    if (MI.isInlineAsm())
      return false;
    // Iterations 2..n of the original loop entered with the decrement's
    // flags; a body instruction reading CFS before any body definition
    // carries that dependence and cannot be preserved.
    if (!SeenCFSDef && MI.readsRegister(EZH::CFS, TRI))
      return false;
    if (MI.definesRegister(EZH::CFS, TRI))
      SeenCFSDef = true;
    // The counter must be invisible to the body.
    if (MI.readsRegister(Cnt, TRI) || MI.definesRegister(Cnt, TRI))
      return false;
    // No writes to the special control registers that alter control flow.
    if (MI.definesRegister(EZH::PC, TRI) || MI.definesRegister(EZH::RA, TRI) ||
        MI.definesRegister(EZH::SP, TRI))
      return false;
    for (const MachineOperand &MO : MI.operands())
      if (MO.isCPI())
        ++BodyPoolUsers;
    ++BodySize;
  }
  if (BodySize < 1 || BodySize > TightLoopMaxBody)
    return false;
  // Every pool user in the body adds an entry to the island the
  // constant-island pass will place immediately BEFORE the loop; the setup
  // load at the loop head must keep that island within its -512-byte reach.
  // Budget: setup (16B) + body + island entries (ours + the body's, plus the
  // branch-around goto) comfortably inside the reach.
  if (16 + BodySize * 4 + (BodyPoolUsers + 2) * 4 + 4 > 500)
    return false;

  // A scratch GPR for Rend, free on entry to the loop block.
  LivePhysRegs LPR(*TRI);
  LPR.addLiveIns(*B);
  Register REnd;
  for (MCPhysReg R : {EZH::R0, EZH::R1, EZH::R2, EZH::R3, EZH::R4, EZH::R5,
                      EZH::R6, EZH::R7}) {
    if (R != Cnt && LPR.available(MF.getRegInfo(), R)) {
      REnd = R;
      break;
    }
  }
  if (!REnd)
    return false;

  // --- Rewrite ---
  // Rend: a constant-pool entry holding the exit block's address (the same
  // CPMachineBasicBlock mechanism the jump tables and branch fixups use).
  auto *CPV = new EZHConstantPoolValue(
      Exit, Type::getInt32Ty(MF.getFunction().getContext()));
  unsigned CPI = MF.getConstantPool()->getConstantPoolIndex(CPV, Align(4));
  Exit->setMachineBlockAddressTaken();
  Exit->setLabelMustBeEmitted();

  DebugLoc DL = TI->getDebugLoc();
  MachineBasicBlock::iterator InsPt = B->begin();
  BuildMI(*B, InsPt, DL, TII->get(EZH::LOAD_CONSTANT), REnd)
      .addConstantPoolIndex(CPI);
  BuildMI(*B, InsPt, DL, TII->get(EZH::ADD_IMM), Cnt)
      .addReg(Cnt)
      .addImm(-1)
      .addImm(EZHCC::ICC_EU);
  BuildMI(*B, InsPt, DL, TII->get(EZH::TIGHT_LOOP))
      .addReg(REnd, RegState::Kill)
      .addReg(Cnt, RegState::Kill)
      .addImm(EZHCC::ICC_EU);
  // The instruction after tight_loop executes exactly once (AN14650); keep
  // the repeated block = the original body by parking a nop in that slot.
  BuildMI(*B, InsPt, DL, TII->get(EZH::NOP));

  Dec->eraseFromParent();
  TI->eraseFromParent();
  B->removeSuccessor(B);

  // Never let the constant-island pass insert an island after this block
  // (its data would sit inside the repeated region) or split it.
  MF.getInfo<EZHMachineFunctionInfo>()->addTightLoopBody(B);

  ++NumTightLoops;
  return true;
}

namespace llvm {
FunctionPass *createEZHTightLoopFormationPass() {
  return new EZHTightLoopFormation();
}
} // namespace llvm
