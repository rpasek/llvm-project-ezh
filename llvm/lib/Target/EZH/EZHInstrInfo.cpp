//===-- EZHInstrInfo.cpp - EZH Instruction Information ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the EZH implementation of the TargetInstrInfo class.
//
// Description:
//   Implements branch analysis (analyzeBranch), branch modification
//   (insertBranch, removeBranch, reverseBranchCondition), and instruction
//   property queries.
//
// Copied From:
//   Lanai target backend (llvm/lib/Target/Lanai/LanaiInstrInfo.cpp).
//
// Changes:
//   Customized analyzeBranch to decode EZH conditional branch encodings and
//   preceding condition codes; implemented branch reversal handling EZH flag
//   semantics.
//
//===----------------------------------------------------------------------===//

#include "EZHInstrInfo.h"
#include "EZHCondCode.h"
#include "EZHSubtarget.h"
#include "MCTargetDesc/EZHBaseInfo.h"
#include "MCTargetDesc/EZHMCTargetDesc.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Target/TargetMachine.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#include "EZHGenInstrInfo.inc"

#define DEBUG_TYPE "ezh-instr-info"

EZHInstrInfo::EZHInstrInfo(const EZHSubtarget &STI)
    : EZHGenInstrInfo(STI, RegisterInfo, EZH::ADJCALLSTACKDOWN,
                      EZH::ADJCALLSTACKUP),
      RegisterInfo() {}

void EZHInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator Position,
                               const DebugLoc &DL, Register DestinationRegister,
                               Register SourceRegister, bool KillSource,
                               bool RenamableDest, bool RenamableSrc) const {
  if (!EZH::GPRRegClass.contains(DestinationRegister, SourceRegister)) {
    llvm_unreachable("Impossible reg-to-reg copy");
  }

  BuildMI(MBB, Position, DL, get(EZH::MOV), DestinationRegister)
      .addReg(SourceRegister, getKillRegState(KillSource))
      .addImm(EZHCC::ICC_EU);
}

void EZHInstrInfo::storeRegToStackSlot(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator Position,
                                       Register SourceRegister, bool IsKill,
                                       int FrameIndex,
                                       const TargetRegisterClass *RegisterClass,
                                       Register /*VReg*/,
                                       MachineInstr::MIFlag Flags) const {
  DebugLoc DL;
  if (Position != MBB.end()) {
    DL = Position->getDebugLoc();
  }

  if (!EZH::GPRRegClass.hasSubClassEq(RegisterClass)) {
    llvm_unreachable("Can't store this register to stack slot");
  }
  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIndex),
      MachineMemOperand::MOStore, MFI.getObjectSize(FrameIndex),
      MFI.getObjectAlign(FrameIndex));
  BuildMI(MBB, Position, DL, get(EZH::STR))
      .addReg(SourceRegister, getKillRegState(IsKill))
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .addImm(EZHCC::ICC_EU)
      .addMemOperand(MMO)
      .setMIFlags(Flags);
}

void EZHInstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator Position,
    Register DestinationRegister, int FrameIndex,
    const TargetRegisterClass *RegisterClass, Register /*VReg*/,
    unsigned /*SubReg*/, MachineInstr::MIFlag Flags) const {
  DebugLoc DL;
  if (Position != MBB.end()) {
    DL = Position->getDebugLoc();
  }

  if (!EZH::GPRRegClass.hasSubClassEq(RegisterClass)) {
    llvm_unreachable("Can't load this register from stack slot");
  }
  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIndex),
      MachineMemOperand::MOLoad, MFI.getObjectSize(FrameIndex),
      MFI.getObjectAlign(FrameIndex));
  BuildMI(MBB, Position, DL, get(EZH::LDR), DestinationRegister)
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .addImm(EZHCC::ICC_EU)
      .addMemOperand(MMO)
      .setMIFlags(Flags);
}

bool EZHInstrInfo::expandPostRAPseudo(MachineInstr &MI) const {
  // The receiver clobber has done its job once registers are allocated.
  if (MI.getOpcode() == EZH::SJLJ_RECEIVER_CLOBBER) {
    MI.eraseFromParent();
    return true;
  }
  return false;
}

bool EZHInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                 MachineBasicBlock *&TrueBlock,
                                 MachineBasicBlock *&FalseBlock,
                                 SmallVectorImpl<MachineOperand> &Condition,
                                 bool AllowModify) const {
  TrueBlock = nullptr;
  FalseBlock = nullptr;
  unsigned NumTerminatorsSeen = 0;

  MachineBasicBlock::iterator I = MBB.end();
  if (I == MBB.begin())
    return false;
  --I;

  while (I->isTerminator() || I->isDebugInstr()) {
    // Skip debug instructions.
    while (I->isDebugInstr()) {
      if (I == MBB.begin())
        return false;
      --I;
    }
    if (!I->isTerminator())
      break;

    ++NumTerminatorsSeen;

    if (I->getOpcode() == EZH::GOTO) {
      if (!I->getOperand(0).isMBB())
        return true;

      unsigned CC = I->getOperand(1).getImm();
      if (CC != EZHCC::ICC_EU) {
        // Conditional Branch
        if (!Condition.empty())
          return true; // Only support one conditional branch

        FalseBlock = TrueBlock;
        TrueBlock = I->getOperand(0).getMBB();
        Condition.push_back(MachineOperand::CreateImm(CC));
      } else {
        // Unconditional Branch
        if (NumTerminatorsSeen > 1) {
          // We already saw a branch (which must be conditional, since we scan
          // upwards and we don't support multiple unconditional branches). If
          // we see an unconditional branch before a conditional one, it is
          // invalid CFG.
          return true;
        }

        TrueBlock = I->getOperand(0).getMBB();
        Condition.clear();
        FalseBlock = nullptr;
      }
    } else {
      // Unrecognized terminator.
      return true;
    }

    // Cleanup code - only for unconditional branches that are the last
    // instruction.
    if (I->getOpcode() == EZH::GOTO &&
        I->getOperand(1).getImm() == EZHCC::ICC_EU) {
      if (NumTerminatorsSeen > 1) {
        if (AllowModify) {
          MachineBasicBlock::iterator DI = std::next(I);
          while (DI != MBB.end()) {
            MachineInstr &InstToDelete = *DI;
            ++DI;
            InstToDelete.eraseFromParent();
          }
          NumTerminatorsSeen = 1;
          TrueBlock = I->getOperand(0).getMBB();
          Condition.clear();
          FalseBlock = nullptr;
        } else {
          return true;
        }
      }
    }

    if (I == MBB.begin())
      return false;
    --I;
  }

  return false;
}

unsigned EZHInstrInfo::insertBranch(MachineBasicBlock &MBB,
                                    MachineBasicBlock *TrueBlock,
                                    MachineBasicBlock *FalseBlock,
                                    ArrayRef<MachineOperand> Condition,
                                    const DebugLoc &DL, int *BytesAdded) const {
  if (BytesAdded)
    *BytesAdded = 0;

  if (Condition.empty()) {
    BuildMI(&MBB, DL, get(EZH::GOTO)).addMBB(TrueBlock).addImm(EZHCC::ICC_EU);
    if (BytesAdded)
      *BytesAdded += 4;
    return 1;
  }

  unsigned CC = Condition[0].getImm();
  BuildMI(&MBB, DL, get(EZH::GOTO)).addMBB(TrueBlock).addImm(CC);
  if (BytesAdded)
    *BytesAdded += 4;

  if (FalseBlock) {
    BuildMI(&MBB, DL, get(EZH::GOTO)).addMBB(FalseBlock).addImm(EZHCC::ICC_EU);
    if (BytesAdded)
      *BytesAdded += 4;
    return 2;
  }

  return 1;
}

unsigned EZHInstrInfo::removeBranch(MachineBasicBlock &MBB,
                                    int *BytesRemoved) const {
  if (BytesRemoved)
    *BytesRemoved = 0;

  MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
  if (I == MBB.end())
    return 0;

  if (!I->isBranch())
    return 0;

  // Remove the last branch (unconditional or conditional).
  I->eraseFromParent();
  if (BytesRemoved)
    *BytesRemoved += 4;
  unsigned Count = 1;

  I = MBB.getLastNonDebugInstr();
  if (I == MBB.end()) {
    return Count;
  }
  if (!I->isBranch()) {
    return Count;
  }

  // Remove the joint conditional branch.
  I->eraseFromParent();
  if (BytesRemoved)
    *BytesRemoved += 4;
  return Count + 1;
}

bool EZHInstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert(Cond.size() == 1 && "Invalid branch condition!");
  unsigned CC = Cond[0].getImm();
  EZHCC::CondCode RevCC =
      EZHCC::getReversedCondCode(static_cast<EZHCC::CondCode>(CC));
  if (RevCC == EZHCC::UNKNOWN)
    return true;
  Cond[0].setImm(RevCC);
  return false;
}

bool EZHInstrInfo::isPredicated(const MachineInstr &MI) const {
  int PIdx = -1;
  const MCInstrDesc &MCID = MI.getDesc();
  for (unsigned i = 0, e = MCID.getNumOperands(); i != e; ++i) {
    if (MCID.operands()[i].Flags & (1 << MCOI::Predicate)) {
      PIdx = i;
      break;
    }
  }
  if (PIdx >= 0) {
    if (static_cast<unsigned>(PIdx) < MI.getNumOperands())
      return MI.getOperand(PIdx).getImm() != EZHCC::ICC_EU;
    return false; // Malformed instruction, assume not predicated to avoid crash
  }

  return (MCID.TSFlags & EZHII::IsPredicated) != 0;
}

// The rematerializable materialization instructions (load_imm/load_simm
// families, LOAD_CONSTANT) carry their predicate as an operand and share
// their opcode with the predicated encodings; only the unpredicated form
// may be re-executed at an arbitrary program point.
bool EZHInstrInfo::isReMaterializableImpl(const MachineInstr &MI) const {
  if (isPredicated(MI))
    return false;
  switch (MI.getOpcode()) {
  case EZH::LOAD_IMM:
  case EZH::LOAD_IMMN:
  case EZH::LOAD_SIMM:
  case EZH::LOAD_SIMMN:
    // Unpredicated immediate materializations are pure value producers.
    // Their descriptors keep hasSideEffects = 1 (the predicated instances
    // sharing the opcode read unmodelled flags, and the conservative flag
    // is what pins them against any future late motion pass), which the
    // generic check below would reject -- accept them here instead.
    return true;
  default:
    return TargetInstrInfo::isReMaterializableImpl(MI);
  }
}

bool EZHInstrInfo::isPredicable(const MachineInstr &MI) const {
  return (MI.getDesc().TSFlags & EZHII::IsPredicable) != 0;
}

bool EZHInstrInfo::canPredicatePredicatedInstr(const MachineInstr &MI) const {
  return false;
}

bool EZHInstrInfo::PredicateInstruction(MachineInstr &MI,
                                        ArrayRef<MachineOperand> Pred) const {
  assert(!Pred.empty() && "Empty predicate!");
  EZHCC::CondCode CC = static_cast<EZHCC::CondCode>(Pred[0].getImm());

  const MCInstrDesc &MCID = MI.getDesc();
  int PIdx = -1;
  for (unsigned i = 0, e = MCID.getNumOperands(); i != e; ++i) {
    if (MCID.operands()[i].Flags & (1 << MCOI::Predicate)) {
      PIdx = i;
      break;
    }
  }

  if (PIdx >= 0) {
    if (static_cast<unsigned>(PIdx) < MI.getNumOperands()) {
      MI.getOperand(PIdx).setImm(CC);

      // Add implicit use of the destination register to preserve its value
      // if the condition is false!
      if (MI.getNumOperands() > 0 && MI.getOperand(0).isReg() &&
          MI.getOperand(0).isDef()) {
        Register RdReg = MI.getOperand(0).getReg();
        MI.addOperand(
            MachineOperand::CreateReg(RdReg, /*isDef=*/false, /*isImp=*/true));
      }
      return true;
    }
    return false; // Malformed instruction, cannot predicate
  }

  return false;
}

bool EZHInstrInfo::isProfitableToIfCvt(MachineBasicBlock &MBB,
                                       unsigned NumCycles,
                                       unsigned ExtraPredCycles,
                                       BranchProbability Probability) const {
  return true;
}

bool EZHInstrInfo::isProfitableToIfCvt(
    MachineBasicBlock &TMBB, unsigned NumTCycles, unsigned ExtraTCycles,
    MachineBasicBlock &FMBB, unsigned NumFCycles, unsigned ExtraFCycles,
    BranchProbability Probability) const {
  return true;
}

unsigned EZHInstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  if (MI.isInlineAsm()) {
    const MachineFunction *MF = MI.getParent()->getParent();
    const MCAsmInfo &MAI = MF->getTarget().getMCAsmInfo();
    unsigned Size = getInlineAsmLength(MI.getOperand(0).getSymbolName(), MAI);
    return alignTo(Size, 4);
  }
  if (MI.getOpcode() == EZH::CONSTPOOL_ENTRY) {
    return MI.getOperand(2).getImm();
  }
  unsigned Size = MI.getDesc().getSize();
  if (Size > 0)
    return Size;
  if (MI.isMetaInstruction())
    return 0;
  return 4;
}

int EZHInstrInfo::getJumpTableIndex(const MachineInstr &MI) const {
  if (MI.getOpcode() == EZH::PseudoBR_JT) {
    const MachineOperand &MO = MI.getOperand(2);
    if (MO.isJTI()) {
      return MO.getIndex();
    } else if (MO.isImm()) {
      return MO.getImm();
    }
  }
  return -1;
}
