//===-- EZHCompareFusion.cpp - Fold compare-with-zero into S-forms ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Description:
//   EZH has no dedicated compare instruction; ISel materializes flag tests as
//   a flag-setting subtract with a dead destination:
//
//     add_imm  r2, r2, -1        ; ALU op producing the tested value
//     sub_imms r3, r2, 0         ; compare-with-zero, r3 dead
//     goto_nz  .loop
//
//   Every ALU instruction has an S-twin whose Z and N flags are computed
//   from the result value itself, which is exactly what the compare-with-
//   zero re-derives. This pass rewrites the producer to its S-twin and
//   erases the compare:
//
//     add_imms r2, r2, -1
//     goto_nz  .loop
//
//   Only the carry flag can differ between the twin and the erased compare,
//   so the fold is applied only when every possibly-flag-reading instruction
//   from the compare to the end of the block uses a condition derived purely
//   from the result value (ZE/NZ/PO/NE). ADC/SBC read carry as data without
//   a predicate operand and therefore also block the fold.
//
//   The producer does not have to be adjacent: the pass walks backwards
//   over flag-transparent instructions (loop bookkeeping commonly sits
//   between the ALU op and its test), since after the fold those execute
//   between the S-twin's flag write and the consumers.
//
// Copied From:
//   Newly authored custom file for the EZH (SmartDMA) target architecture
//
// Changes:
//   Authored from scratch.
//
//===----------------------------------------------------------------------===//

#include "EZH.h"
#include "EZHCondCode.h"
#include "EZHInstrInfo.h"
#include "EZHSubtarget.h"
#include "MCTargetDesc/EZHMCTargetDesc.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

using namespace llvm;

#define DEBUG_TYPE "ezh-compare-fusion"

namespace {

// The predicate operand index, or -1 if the instruction has none.
static int getPredIdx(const MachineInstr &MI) {
  const MCInstrDesc &MCID = MI.getDesc();
  for (unsigned i = 0, e = MCID.getNumOperands(); i != e; ++i)
    if (MCID.operands()[i].Flags & (1 << MCOI::Predicate))
      return i;
  return -1;
}

// S-twin for producers whose Z and N flags equal those of a subsequent
// compare-with-zero of their result. True of every ALU op (Z = result == 0,
// N = result bit 31); listed explicitly so nothing slips in by accident.
static unsigned flagSettingTwin(unsigned Opc) {
  switch (Opc) {
  case EZH::ADD:      return EZH::ADD_s;
  case EZH::ADD_IMM:  return EZH::ADD_IMM_s;
  case EZH::SUB:      return EZH::SUB_s;
  case EZH::SUB_IMM:  return EZH::SUB_IMM_s;
  case EZH::AND:      return EZH::AND_s;
  case EZH::AND_IMM:  return EZH::AND_IMM_s;
  case EZH::OR:       return EZH::OR_s;
  case EZH::OR_IMM:   return EZH::OR_IMM_s;
  case EZH::XOR:      return EZH::XOR_s;
  case EZH::XOR_IMM:  return EZH::XOR_IMM_s;
  case EZH::LSL:      return EZH::LSL_s;
  case EZH::LSR:      return EZH::LSR_s;
  case EZH::ASR:      return EZH::ASR_s;
  case EZH::ROR:      return EZH::ROR_s;
  case EZH::RLSL:     return EZH::RLSL_s;
  case EZH::RLSR:     return EZH::RLSR_s;
  case EZH::RASR:     return EZH::RASR_s;
  case EZH::RROR:     return EZH::RROR_s;
  case EZH::MOV:      return EZH::MOV_s;
  default:            return 0;
  }
}

// Conditions computed from the result value only (zero and sign), which the
// producer's S-twin reproduces exactly. Everything else (carry family,
// combiner conditions, anything with unclear semantics) blocks the fold.
static bool isResultOnlyCC(int64_t CC) {
  switch (CC) {
  case EZHCC::ICC_ZE:
  case EZHCC::ICC_NZ:
  case EZHCC::ICC_PO:
  case EZHCC::ICC_NE:
    return true;
  default:
    return false;
  }
}

class EZHCompareFusion : public MachineFunctionPass {
public:
  inline static char ID = 0;
  EZHCompareFusion() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override { return "EZH compare fusion"; }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  const TargetInstrInfo *TII = nullptr;

  bool tryFuse(MachineBasicBlock &MBB, MachineInstr &Cmp);
  bool isDeadAfter(Register Reg, const MachineInstr &From) const;
  bool isFlagBarrier(const MachineInstr &MI) const;
};

// True if MI writes the ALU flags (all flag-setting opcodes carry the _s
// suffix), reads them (predication, or carry as data in the adc/sbc
// families), or has effects we cannot see through (calls, branches, inline
// asm). Such an instruction may not sit between a producer moved to its
// S-twin and the consumers of the erased compare's flags.
bool EZHCompareFusion::isFlagBarrier(const MachineInstr &MI) const {
  if (MI.isCall() || MI.isBranch() || MI.isTerminator() || MI.isInlineAsm())
    return true;
  StringRef Name = TII->getName(MI.getOpcode());
  if (Name.ends_with("_s") || Name.starts_with("ADC") ||
      Name.starts_with("SBC"))
    return true;
  int PIdx = getPredIdx(MI);
  return PIdx >= 0 && MI.getOperand(PIdx).getImm() != EZHCC::ICC_EU;
}

// True if Reg is not read on any path after From (exclusive). Trusts the
// dead flag when present, otherwise scans to the block end and falls back to
// successor live-ins, which are maintained post-RA.
bool EZHCompareFusion::isDeadAfter(Register Reg,
                                   const MachineInstr &From) const {
  const MachineBasicBlock &MBB = *From.getParent();
  for (MachineBasicBlock::const_iterator I = std::next(From.getIterator()),
                                         E = MBB.end();
       I != E; ++I) {
    if (I->readsRegister(Reg, /*TRI=*/nullptr))
      return false;
    if (I->definesRegister(Reg, /*TRI=*/nullptr))
      return true;
  }
  for (const MachineBasicBlock *Succ : MBB.successors())
    if (Succ->isLiveIn(Reg))
      return false;
  return true;
}

bool EZHCompareFusion::tryFuse(MachineBasicBlock &MBB, MachineInstr &Cmp) {
  // Cmp must be an unpredicated compare-with-zero: sub_imms rD, rS, 0.
  // Operand layout: $Rd, $Rs1, $Imm, $Cond.
  if (Cmp.getOpcode() != EZH::SUB_IMM_s)
    return false;
  if (!Cmp.getOperand(2).isImm() || Cmp.getOperand(2).getImm() != 0)
    return false;
  if (Cmp.getOperand(3).getImm() != EZHCC::ICC_EU)
    return false;

  Register CmpDst = Cmp.getOperand(0).getReg();
  Register Tested = Cmp.getOperand(1).getReg();
  if (!(Cmp.getOperand(0).isDead() || isDeadAfter(CmpDst, Cmp)))
    return false;

  // Find the producer of the tested value by walking backwards from the
  // compare. The first definition of the tested register is the producer;
  // every instruction skipped on the way must be flag-transparent, since
  // after the fold it will execute between the S-twin's flag write and the
  // consumers.
  MachineInstr *Producer = nullptr;
  MachineBasicBlock::iterator P = Cmp.getIterator();
  for (unsigned Steps = 0; P != MBB.begin() && Steps < 8; ++Steps) {
    --P;
    if (P->modifiesRegister(Tested, /*TRI=*/nullptr)) {
      Producer = &*P;
      break;
    }
    if (isFlagBarrier(*P))
      return false;
  }
  if (!Producer)
    return false;
  unsigned Twin = flagSettingTwin(Producer->getOpcode());
  if (!Twin)
    return false;
  if (!Producer->getOperand(0).isReg() ||
      Producer->getOperand(0).getReg() != Tested)
    return false;
  int ProdPredIdx = getPredIdx(*Producer);
  if (ProdPredIdx < 0 ||
      Producer->getOperand(ProdPredIdx).getImm() != EZHCC::ICC_EU)
    return false;

  // Every instruction from the compare to the end of the block that could
  // read the flags must be satisfied by the S-twin: predicated instructions
  // must use result-only conditions, and carry-as-data readers (adc/sbc
  // families) block the fold outright. Flags never live across EZH block
  // boundaries (ISel and if-conversion keep compare and consumer in one
  // block), so the scan stops there. This is conservative past the next
  // flag writer, which can only reject a legal fold, never accept a bad one.
  for (MachineBasicBlock::iterator I = std::next(Cmp.getIterator()),
                                   E = MBB.end();
       I != E; ++I) {
    StringRef Name = TII->getName(I->getOpcode());
    if (Name.starts_with("ADC") || Name.starts_with("SBC"))
      return false;
    int PIdx = getPredIdx(*I);
    if (PIdx < 0)
      continue;
    int64_t CC = I->getOperand(PIdx).getImm();
    if (CC != EZHCC::ICC_EU && !isResultOnlyCC(CC))
      return false;
  }

  Producer->setDesc(TII->get(Twin));
  Cmp.eraseFromParent();
  return true;
}

bool EZHCompareFusion::runOnMachineFunction(MachineFunction &MF) {
  TII = MF.getSubtarget().getInstrInfo();

  bool Changed = false;
  for (auto &MBB : MF)
    for (auto I = MBB.begin(), E = MBB.end(); I != E;) {
      MachineInstr &MI = *I;
      ++I; // tryFuse may erase MI.
      Changed |= tryFuse(MBB, MI);
    }
  return Changed;
}

} // namespace

namespace llvm {
FunctionPass *createEZHCompareFusionPass() { return new EZHCompareFusion(); }
} // namespace llvm
