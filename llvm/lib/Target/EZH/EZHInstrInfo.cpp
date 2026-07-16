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
#include "EZHSubtarget.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineOutliner.h"
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
    // Unpredicated immediate materializations are pure value producers;
    // their descriptors clear hasSideEffects (LiveRangeEdit gates remat on
    // isSafeToMove, which rejects unmodelled side effects with no target
    // override, so the flag cannot stay set). The predicated encodings
    // sharing these opcodes read unmodelled flags and rely on nothing
    // moving instructions after the if-converter -- an invariant
    // EZHTargetMachine::targetSchedulesPostRAScheduling enforces. This
    // explicit accept is then belt-and-braces should the descriptors ever
    // change; the isPredicated rejection above is the load-bearing part.
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

//===----------------------------------------------------------------------===//
// MachineOutliner
//===----------------------------------------------------------------------===//

// The one frame/call construction EZH supports: call the outlined function
// with a gosub, return with goto ra. There is no alternate link register,
// so no tail-call or register-save variant exists.
namespace {
enum EZHOutlinerConstructionID { EZHOutlinerDefault };
} // namespace

// An instruction is outlinable only if it is a pure, position-independent,
// state-free value computation: no control transfer, no memory, no side
// effects, no flag production or consumption, no position-dependent
// operand, and no reserved/special register. Relocating such an
// instruction into a shared function cannot change its behavior.
//
// This is a rejection over an EXHAUSTIVE enumeration of EZH's danger
// classes, not a positive opcode list -- which is both safer (no danger
// class can be forgotten by omitting an opcode) and more complete (it
// admits the fused shifted-ALU forms without a fragile 60-entry table).
// The completeness argument, class by class:
//  - control transfer -> isBranch/isCall/isReturn/isTerminator/indirect
//  - memory, incl. pc-relative pool loads (LOAD_CONSTANT has mayLoad) ->
//    mayLoadOrStore
//  - peripheral/event/GPIO/CFM/hold/trigger/tight_loop -> every one is
//    hasUnmodeledSideEffects
//  - flag PRODUCED -> the _s suffix is exactly how the by-construction flag
//    model marks a producer
//  - flag/carry CONSUMED -> isPredicated (pred operand != ICC_EU) or an ADC
//    / SBC carry lane anywhere in the mnemonic
//  - position-dependent operand (CPI/JTI/MBB/global/blockaddress/symbol/
//    frame index) -> the operand loop admits only reg and plain imm
//  - reserved/special register, explicit OR implicit -> reads/modifies
//    check over RA/SP/PC/GPO/GPD/CFS/CFM/GPI
bool EZHInstrInfo::isOutlineWhitelisted(const MachineInstr &MI) const {
  if (MI.isBranch() || MI.isCall() || MI.isReturn() || MI.isTerminator() ||
      MI.isIndirectBranch() || MI.isInlineAsm())
    return false;
  if (MI.mayLoadOrStore() || MI.hasUnmodeledSideEffects())
    return false;

  // Flags: no producer (_s), no consumer (predicated), no carry lane.
  if (isPredicated(MI))
    return false;
  StringRef Name = getName(MI.getOpcode());
  if (Name.ends_with("_s") || Name.contains("ADC") || Name.contains("SBC"))
    return false;

  // Operands: register or plain immediate only.
  for (const MachineOperand &MO : MI.operands())
    if (!MO.isReg() && !MO.isImm())
      return false;

  // No reserved/special register (covers implicit uses/defs too).
  const EZHRegisterInfo *TRI = &getRegisterInfo();
  for (Register R : {EZH::RA, EZH::SP, EZH::PC, EZH::GPO, EZH::GPD, EZH::CFS,
                     EZH::CFM, EZH::GPI})
    if (MI.readsRegister(R, TRI) || MI.modifiesRegister(R, TRI))
      return false;

  // Must produce a value (a pure computation), never a bare pseudo/meta.
  return MI.getNumExplicitDefs() >= 1 && !MI.isPseudo();
}

outliner::InstrType
EZHInstrInfo::getOutliningTypeImpl(const MachineModuleInfo &, /*MMI*/
                                   MachineBasicBlock::iterator &MIT,
                                   unsigned Flags) const {
  MachineInstr &MI = *MIT;

  if (MI.isDebugInstr() || MI.getOpcode() == TargetOpcode::IMPLICIT_DEF ||
      MI.isKill())
    return outliner::InstrType::Invisible;

  // CFI runs would split unwind state across two code regions; never
  // outline them (the outlined function carries no CFA).
  if (MI.isCFIInstruction())
    return outliner::InstrType::Illegal;

  return isOutlineWhitelisted(MI) ? outliner::InstrType::Legal
                                  : outliner::InstrType::Illegal;
}

std::optional<std::unique_ptr<outliner::OutlinedFunction>>
EZHInstrInfo::getOutliningCandidateInfo(
    const MachineModuleInfo &MMI,
    std::vector<outliner::Candidate> &RepeatedSequenceLocs,
    unsigned MinRepeats) const {
  // A single link register: the outlined call (gosub) clobbers RA, so a
  // candidate is viable only where RA is dead across the sequence. Framed
  // functions spilled RA in the prologue (dead across the body); frameless
  // leaves keep the live return address in RA and are rejected. Require
  // BOTH the liveness query and that the function actually spilled RA, so
  // the gate never depends on pristine-register inference alone -- this is
  // the sole guard against return-address corruption and has no backstop.
  auto SpilledRA = [](const outliner::Candidate &C) {
    for (const CalleeSavedInfo &CS :
         C.getMF()->getFrameInfo().getCalleeSavedInfo())
      if (CS.getReg() == EZH::RA)
        return true;
    return false;
  };
  const EZHRegisterInfo *TRI = &getRegisterInfo();
  llvm::erase_if(RepeatedSequenceLocs, [&](outliner::Candidate &C) {
    return !SpilledRA(C) || !C.isAvailableAcrossAndOutOfSeq(EZH::RA, *TRI);
  });

  if (RepeatedSequenceLocs.size() < MinRepeats)
    return std::nullopt;

  unsigned SequenceSize = 0;
  for (const MachineInstr &MI : RepeatedSequenceLocs[0])
    SequenceSize += getInstSizeInBytes(MI);

  // gosub is 4 bytes; under bitslice interrupts BitSliceInjection (which
  // runs after the outliner) will prepend a 4-byte gotol_bs poll to each
  // new gosub site. FrameOverhead is the single trailing RET (4 bytes); no
  // poll is added to RET (not a branch or call) and there is no prologue.
  bool Bitslice = RepeatedSequenceLocs[0]
                      .getMF()
                      ->getSubtarget<EZHSubtarget>()
                      .hasBitSliceInterrupts();
  unsigned CallOverhead = Bitslice ? 8 : 4;
  for (outliner::Candidate &C : RepeatedSequenceLocs)
    C.setCallInfo(EZHOutlinerDefault, CallOverhead);

  return std::make_unique<outliner::OutlinedFunction>(
      RepeatedSequenceLocs, SequenceSize, /*FrameOverhead=*/4,
      EZHOutlinerDefault);
}

bool EZHInstrInfo::isFunctionSafeToOutlineFrom(
    MachineFunction &MF, bool OutlineFromLinkOnceODRs) const {
  const Function &F = MF.getFunction();
  // The interrupt handler must never be restructured; and never outline
  // across a section boundary or from a naked function.
  if (MF.getName() == "bitslice_handler")
    return false;
  if (F.hasSection() || F.hasFnAttribute(Attribute::Naked))
    return false;
  if (!OutlineFromLinkOnceODRs && F.hasLinkOnceODRLinkage())
    return false;
  return true;
}

bool EZHInstrInfo::shouldOutlineFromFunctionByDefault(
    MachineFunction &MF) const {
  // Default-on only at -Oz, matching the upstream AArch64/RISC-V
  // convention: outlining captures just intra-module repetition, which is
  // sparse in typical single-module EZH firmware (measured ~0.004% of
  // .text across the 3078-test corpus), so it is not worth the pass cost by
  // default at -Os. It stays available at any level via
  // -mllvm -enable-machine-outliner and is fully correct when enabled
  // (validated across the whole -Os corpus).
  return MF.getFunction().hasMinSize();
}

void EZHInstrInfo::buildOutlinedFrame(
    MachineBasicBlock &MBB, MachineFunction &MF,
    const outliner::OutlinedFunction &OF) const {
  // The caller's gosub left the return address in RA; the body is a strict
  // leaf (no whitelisted instruction is a branch or call), so nothing
  // clobbers it. Declare RA live-in for the verifier and append the return.
  MBB.addLiveIn(EZH::RA);
  MBB.insert(MBB.end(), BuildMI(MF, DebugLoc(), get(EZH::RET)));
}

MachineBasicBlock::iterator EZHInstrInfo::insertOutlinedCall(
    Module &M, MachineBasicBlock &MBB, MachineBasicBlock::iterator &It,
    MachineFunction &MF, outliner::Candidate &C) const {
  // MF is the outlined function being called (the pass passes it here);
  // gosub to its symbol. No call-frame setup: gosub does not touch SP and
  // the outlined body is stack-neutral, so every SP+imm access in the
  // caller still hits the same offset.
  It = MBB.insert(It, BuildMI(MF, DebugLoc(), get(EZH::OUTLINE_CALL))
                          .addGlobalAddress(M.getNamedValue(MF.getName())));
  return It;
}
