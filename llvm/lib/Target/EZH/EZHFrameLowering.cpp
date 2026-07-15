//===-- EZHFrameLowering.cpp - EZH Frame Information ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the EZH implementation of TargetFrameLowering class.
//
// Description:
//   Implements function prologue and epilogue insertion (emitPrologue,
//   emitEpilogue), managing stack pointer adjustments and callee-saved
//   register spills/restores.
//
//   The EZH stack frame layout matches the Thumb1 layout:
//
//                  |-------------------|
//                  | Incoming Stack    |  (passed by caller)
//                  | Arguments         |
//   entry_SP ->    |-------------------|  (High Address)
//                  | VarArgsSaveArea   |  (allocated first)
//                  +-------------------+
//                  | Saved RA          |  (CSI spill, highest CSR)
//                  +-------------------+
//                  | Saved FP (R7)     |  (CSI spill, FP points here)
//                  +-------------------+
//                  | Saved BP (R6)     |  (CSI spill, Optional)
//                  +-------------------+
//                  | Saved GPRs (R5,R4)|  (CSI spill)
//                  +-------------------+
//                  | Locals            |  (allocated last)
//                  +-------------------+  (Low Address) <--- SP
//
//   Notes on Optional Registers:
//     - FP (R7) is present if hasFP(MF) is true (e.g., forced by user,
//       variable-sized allocas present, or stack realignment required).
//     - BP (R6) is present if hasBasePointer(MF) is true (e.g., stack
//       realignment is required).
//
//   Notes on VarArgs:
//     - VarArgsSaveArea is allocated at the very top of the stack frame (just
//       below entry_SP).
//     - Register-passed arguments that are part of the variable argument list
//       are spilled into this area in the prologue.
//     - This creates a contiguous memory region with any stack-passed
//       arguments (which reside at entry_SP and above), allowing va_list to
//       access them sequentially.
//
//   Prologue Allocation Sequence:
//     - If VarArgs: SP is adjusted by VarArgsSaveSize.
//     - Callee-saved registers (including RA, FP, and BP if needed) are pushed
//       using STR_PRE.
//     - If hasFP: FP (R7) is set to point to the saved FP slot.
//     - SP is adjusted for local variables.
//
//   Epilogue Deallocation Sequence:
//     - If hasFP: SP is restored to the bottom of CSRs using FP.
//     - Else: SP is adjusted by LocalSize.
//     - Callee-saved registers are popped using LDR_POST.
//     - If VarArgs: SP is adjusted to deallocate VarArgsSaveArea.
//
// Copied From:
//   Lanai target backend (llvm/lib/Target/Lanai/LanaiFrameLowering.cpp).
//
// Changes:
//   Replaced Lanai prologue/epilogue instructions with EZH stack pointer
//   adjustments and register spill/restore sequences.
//
//===----------------------------------------------------------------------===//

#include "EZHFrameLowering.h"
#include "EZHCondCode.h"
#include "EZHInstrInfo.h"
#include "EZHMachineFunctionInfo.h"
#include "EZHSubtarget.h"
#include "MCTargetDesc/EZHMCTargetDesc.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/bit.h"
#include "llvm/CodeGen/CFIInstBuilder.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include <algorithm>
#include <iterator>

using namespace llvm;

bool EZHFrameLowering::spillCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    ArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  if (CSI.empty())
    return false;

  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();

  MachineFunction &MF = *MBB.getParent();
  const EZHInstrInfo &TII =
      *static_cast<const EZHInstrInfo *>(MF.getSubtarget().getInstrInfo());



  CFIInstBuilder CFI(MBB, MI, MachineInstr::FrameSetup);
  EZHMachineFunctionInfo *FuncInfo = MF.getInfo<EZHMachineFunctionInfo>();
  int64_t CFAOffset = FuncInfo->getVarArgsSaveSize();

  for (const CalleeSavedInfo &CS : CSI) {
    unsigned Reg = CS.getReg();
    // Add instruction to push register (STR_PRE with -4 offset)
    BuildMI(MBB, MI, DL, TII.get(EZH::STR_PRE), EZH::SP)
        .addReg(Reg, getKillRegState(true))
        .addReg(EZH::SP)
        .addImm(-4)
        .addImm(EZHCC::ICC_EU)
        .setMIFlag(MachineInstr::FrameSetup);
    CFAOffset += 4;
    CFI.buildDefCFAOffset(CFAOffset);
    CFI.buildOffset(Reg, -CFAOffset);
  }
  return true;
}

bool EZHFrameLowering::restoreCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    MutableArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  if (CSI.empty())
    return false;

  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();

  MachineFunction &MF = *MBB.getParent();
  const EZHInstrInfo &TII =
      *static_cast<const EZHInstrInfo *>(MF.getSubtarget().getInstrInfo());



  bool HasFP = hasFP(MF);
  CFIInstBuilder CFI(MBB, MI, MachineInstr::FrameDestroy);
  int64_t CFAOffset = CSI.size() * 4;

  const CalleeSavedInfo *FirstCSI = CSI.empty() ? nullptr : &CSI.front();
  for (const CalleeSavedInfo &Info : llvm::reverse(CSI)) {
    unsigned Reg = Info.getReg();

    if (Reg == EZH::RA && &Info == FirstCSI && MI != MBB.end() &&
        MI->isReturn() &&
        !MF.getInfo<EZHMachineFunctionInfo>()->getVarArgsSaveSize()) {
      // Pop directly into PC to return in a single instruction!
      MachineInstrBuilder MIB =
          BuildMI(MBB, MI, DL, TII.get(EZH::LDR_POST), EZH::PC)
              .addReg(EZH::SP, RegState::Define)
              .addReg(EZH::SP)
              .addImm(4)
              .addImm(EZHCC::ICC_EU)
              .setMIFlag(MachineInstr::FrameDestroy);

      // Propagate all return registers (R0/R1) from RET to preserve liveness!
      for (const MachineOperand &MO : MI->operands()) {
        if (MO.isReg() && MO.isUse()) {
          MIB.addReg(MO.getReg(), RegState::Implicit);
        }
      }

      // Erase the redundant return terminator (goto ra)
      MBB.erase(MI);
      return true;
    }

    // Add instruction to pop register (LDR_POST with +4 offset)
    BuildMI(MBB, MI, DL, TII.get(EZH::LDR_POST), Reg)
        .addReg(EZH::SP, RegState::Define)
        .addReg(EZH::SP)
        .addImm(4)
        .addImm(EZHCC::ICC_EU)
        .setMIFlag(MachineInstr::FrameDestroy);

    if (!HasFP) {
      CFAOffset -= 4;
      CFI.buildDefCFAOffset(CFAOffset);
    }
    CFI.buildRestore(Reg);
  }
  return true;
}
void EZHFrameLowering::emitPrologue(MachineFunction &MF,
                                    MachineBasicBlock &MBB) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const EZHInstrInfo &TII = *STI.getInstrInfo();
  MachineBasicBlock::iterator MBBI = MBB.begin();
  DebugLoc DL;

  EZHMachineFunctionInfo *FuncInfo = MF.getInfo<EZHMachineFunctionInfo>();
  unsigned VarArgsSaveSize = FuncInfo->getVarArgsSaveSize();
  const std::vector<CalleeSavedInfo> &CSI = MFI.getCalleeSavedInfo();
  unsigned CSRSize = CSI.size() * 4;

  CFIInstBuilder CFI(MBB, MBBI, MachineInstr::FrameSetup);
  int64_t CFAOffset = 0;

  // Allocate VarArg save area first (before pushes)
  if (VarArgsSaveSize) {
    BuildMI(MBB, MBBI, DL, TII.get(EZH::SUB_IMM), EZH::SP)
        .addReg(EZH::SP)
        .addImm(VarArgsSaveSize)
        .addImm(EZHCC::ICC_EU)
        .setMIFlag(MachineInstr::FrameSetup);
    CFAOffset += VarArgsSaveSize;
    CFI.buildDefCFAOffset(CFAOffset);
  }

  // Skip any CSR PUSH instructions already inserted
  while (MBBI != MBB.end() && MBBI->getFlag(MachineInstr::FrameSetup) &&
         (MBBI->getOpcode() == EZH::STR_PRE ||
          MBBI->getOpcode() == TargetOpcode::CFI_INSTRUCTION)) {
    ++MBBI;
  }

  // Update CFAOffset to include CSRs because they are already pushed at this point in the instruction stream.
  CFAOffset += CSRSize;

  // Set up FP (R7) if hasFP is true.
  // FP points to the saved FP slot.
  if (hasFP(MF)) {
    // Find FP spill frame index to get its offset
    int FPFI = 0;
    [[maybe_unused]] bool FoundFP = false;
    for (const auto &Info : CSI) {
      if (Info.getReg() == EZH::R7) {
        FPFI = Info.getFrameIdx();
        FoundFP = true;
        break;
      }
    }
    assert(FoundFP && "FP not found in CSI!");

    // FP offset from SP (after pushes) is:
    // getObjectOffset(FPFI) + CSRSize + VarArgsSaveSize
    int FPOffsetInBlock = MFI.getObjectOffset(FPFI) + CSRSize + VarArgsSaveSize;

    if (FPOffsetInBlock == 0) {
      BuildMI(MBB, MBBI, DL, TII.get(EZH::MOV), EZH::R7)
          .addReg(EZH::SP)
          .addImm(EZHCC::ICC_EU)
          .setMIFlag(MachineInstr::FrameSetup);
    } else {
      BuildMI(MBB, MBBI, DL, TII.get(EZH::ADD_IMM), EZH::R7)
          .addReg(EZH::SP)
          .addImm(FPOffsetInBlock)
          .addImm(EZHCC::ICC_EU)
          .setMIFlag(MachineInstr::FrameSetup);
    }

    CFI.buildDefCFARegister(EZH::R7);
    if (FPOffsetInBlock) {
      CFI.buildDefCFA(EZH::R7, CFAOffset - FPOffsetInBlock);
    }
  }

  // Allocate local stack space
  unsigned StackSize = MFI.getStackSize();
  unsigned LocalSize = StackSize - VarArgsSaveSize - CSRSize;

  if (LocalSize > 0) {
    unsigned AllocAmt = 2040;
    while (LocalSize > 0) {
      unsigned Chunk = std::min(LocalSize, AllocAmt);
      BuildMI(MBB, MBBI, DL, TII.get(EZH::SUB_IMM), EZH::SP)
          .addReg(EZH::SP)
          .addImm(Chunk)
          .addImm(EZHCC::ICC_EU)
          .setMIFlag(MachineInstr::FrameSetup);

      if (!hasFP(MF)) {
        CFAOffset += Chunk;
        CFI.buildDefCFAOffset(CFAOffset);
      }

      LocalSize -= Chunk;
    }
  }

  // Stack realignment
  const EZHRegisterInfo *RegInfo =
      static_cast<const EZHRegisterInfo *>(STI.getRegisterInfo());
  if (RegInfo->hasStackRealignment(MF)) {
    Align Mask = MFI.getMaxAlign();
    int64_t MaskVal = -static_cast<int64_t>(Mask.value());
    BuildMI(MBB, MBBI, DL, TII.get(EZH::AND_IMM), EZH::SP)
        .addReg(EZH::SP)
        .addImm(MaskVal)
        .addImm(EZHCC::ICC_EU)
        .setMIFlag(MachineInstr::FrameSetup);
  }

  // Base pointer
  if (RegInfo->hasBasePointer(MF)) {
    BuildMI(MBB, MBBI, DL, TII.get(EZH::MOV), EZH::R6)
        .addReg(EZH::SP)
        .addImm(EZHCC::ICC_EU)
        .setMIFlag(MachineInstr::FrameSetup);
  }
}

void EZHFrameLowering::emitEpilogue(MachineFunction &MF,
                                    MachineBasicBlock &MBB) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const EZHInstrInfo &TII = *STI.getInstrInfo();
  MachineBasicBlock::iterator MBBI = MBB.getLastNonDebugInstr();
  DebugLoc DL;

  if (MBBI != MBB.end())
    DL = MBBI->getDebugLoc();

  const std::vector<CalleeSavedInfo> &CSI = MFI.getCalleeSavedInfo();
  unsigned CSRSize = CSI.size() * 4;

  EZHMachineFunctionInfo *FuncInfo = MF.getInfo<EZHMachineFunctionInfo>();
  unsigned VarArgsSaveSize = FuncInfo->getVarArgsSaveSize();

  unsigned StackSize = MFI.getStackSize();
  unsigned LocalSize = StackSize - VarArgsSaveSize - CSRSize;

  // Find the place before the POP instructions
  MachineBasicBlock::iterator InsertPt = MBB.getFirstTerminator();
  while (InsertPt != MBB.begin()) {
    MachineBasicBlock::iterator Prev = std::prev(InsertPt);
    if (Prev->getOpcode() == TargetOpcode::CFI_INSTRUCTION) {
      InsertPt = Prev;
      continue;
    }
    if (Prev->getOpcode() != EZH::LDR_POST)
      break;
    InsertPt = Prev;
  }

  // Restore SP to the bottom of CSRs
  if (hasFP(MF)) {
    // Find FP spill frame index to get its offset
    int FPFI = 0;
    [[maybe_unused]] bool FoundFP = false;
    for (const auto &Info : CSI) {
      if (Info.getReg() == EZH::R7) {
        FPFI = Info.getFrameIdx();
        FoundFP = true;
        break;
      }
    }
    assert(FoundFP && "FP not found in CSI!");

    // SPOffsetFromFP = VarArgsSaveSize + CSRSize + MFI.getObjectOffset(FPFI)
    int SPOffsetFromFP = VarArgsSaveSize + CSRSize + MFI.getObjectOffset(FPFI);

    if (SPOffsetFromFP == 0) {
      BuildMI(MBB, InsertPt, DL, TII.get(EZH::MOV), EZH::SP)
          .addReg(EZH::R7)
          .addImm(EZHCC::ICC_EU)
          .setMIFlag(MachineInstr::FrameDestroy);
    } else {
      BuildMI(MBB, InsertPt, DL, TII.get(EZH::SUB_IMM), EZH::SP)
          .addReg(EZH::R7)
          .addImm(SPOffsetFromFP)
          .addImm(EZHCC::ICC_EU)
          .setMIFlag(MachineInstr::FrameDestroy);
    }
  } else if (LocalSize > 0) {
    CFIInstBuilder CFI(MBB, InsertPt, MachineInstr::FrameDestroy);
    int64_t CFAOffset = StackSize;
    unsigned DeallocAmt = 2040;
    while (LocalSize > 0) {
      unsigned Chunk = std::min(LocalSize, DeallocAmt);
      BuildMI(MBB, InsertPt, DL, TII.get(EZH::ADD_IMM), EZH::SP)
          .addReg(EZH::SP)
          .addImm(Chunk)
          .addImm(EZHCC::ICC_EU)
          .setMIFlag(MachineInstr::FrameDestroy);

      CFAOffset -= Chunk;
      CFI.buildDefCFAOffset(CFAOffset);

      LocalSize -= Chunk;
    }
  }

  // If VarArgsSaveSize > 0, we need to add VarArgsSaveSize to SP after POPs.
  // We insert this at the end of the block, before the terminators.
  if (VarArgsSaveSize > 0) {
    MachineBasicBlock::iterator Terminator = MBB.getFirstTerminator();
    BuildMI(MBB, Terminator, DL, TII.get(EZH::ADD_IMM), EZH::SP)
        .addReg(EZH::SP)
        .addImm(VarArgsSaveSize)
        .addImm(EZHCC::ICC_EU)
        .setMIFlag(MachineInstr::FrameDestroy);
  }
}
bool EZHFrameLowering::hasReservedCallFrame(const MachineFunction &MF) const {
  return false;
}

static void emitLoad32BitImm(MachineBasicBlock &MBB,
                             MachineBasicBlock::iterator I, const DebugLoc &dl,
                             const TargetInstrInfo &TII, Register Reg,
                             int64_t Val) {
  uint16_t Chunks[4];
  Chunks[0] = (Val >> 30) & 0x3;
  Chunks[1] = (Val >> 20) & 0x3FF;
  Chunks[2] = (Val >> 10) & 0x3FF;
  Chunks[3] = Val & 0x3FF;

  unsigned PendingShift = 0;

  for (unsigned ChunkIdx = 0; ChunkIdx < 4; ++ChunkIdx) {
    uint16_t Chunk = Chunks[ChunkIdx];
    bool ZeroImm = (Chunk == 0);
    unsigned Op = PendingShift ? EZH::ADD_IMM : EZH::LOAD_IMM;

    if (PendingShift && (!ZeroImm || ChunkIdx == 3)) {
      BuildMI(MBB, I, dl, TII.get(EZH::LSL), Reg)
          .addReg(Reg)
          .addImm(PendingShift)
          .addImm(EZHCC::ICC_EU);
      PendingShift = 0;
    }

    if (!ZeroImm) {
      if (Op == EZH::LOAD_IMM) {
        BuildMI(MBB, I, dl, TII.get(Op), Reg)
            .addImm(Chunk)
            .addImm(EZHCC::ICC_EU);
      } else {
        BuildMI(MBB, I, dl, TII.get(Op), Reg)
            .addReg(Reg)
            .addImm(Chunk)
            .addImm(EZHCC::ICC_EU);
      }
    }

    if (PendingShift || !ZeroImm)
      PendingShift += 10;
  }
}

MachineBasicBlock::iterator EZHFrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator I) const {
  const EZHInstrInfo &TII =
      *static_cast<const EZHInstrInfo *>(STI.getInstrInfo());
  if (!hasReservedCallFrame(MF)) {
    MachineInstr &Old = *I;
    DebugLoc dl = Old.getDebugLoc();
    unsigned Amount = Old.getOperand(0).getImm();
    if (Amount != 0) {
      Amount = alignTo(Amount, getStackAlign());
      unsigned Opc = Old.getOpcode();

      if (Amount <= 2047) {
        if (Opc == EZH::ADJCALLSTACKDOWN) {
          BuildMI(MBB, I, dl, TII.get(EZH::SUB_IMM), EZH::SP)
              .addReg(EZH::SP)
              .addImm(Amount)
              .addImm(EZHCC::ICC_EU);
        } else {
          assert(Opc == EZH::ADJCALLSTACKUP && "Unexpected opcode!");
          BuildMI(MBB, I, dl, TII.get(EZH::ADD_IMM), EZH::SP)
              .addReg(EZH::SP)
              .addImm(Amount)
              .addImm(EZHCC::ICC_EU);
        }
      } else {
        if (Opc == EZH::ADJCALLSTACKDOWN) {
          emitLoad32BitImm(MBB, I, dl, TII, EZH::RA, Amount);
          BuildMI(MBB, I, dl, TII.get(EZH::SUB), EZH::SP)
              .addReg(EZH::SP)
              .addReg(EZH::RA)
              .addImm(EZHCC::ICC_EU);
        } else {
          assert(Opc == EZH::ADJCALLSTACKUP && "Unexpected opcode!");
          emitLoad32BitImm(MBB, I, dl, TII, EZH::RA, Amount);
          BuildMI(MBB, I, dl, TII.get(EZH::ADD), EZH::SP)
              .addReg(EZH::SP)
              .addReg(EZH::RA)
              .addImm(EZHCC::ICC_EU);
        }
      }
    }
  }
  return MBB.erase(I);
}

bool EZHFrameLowering::hasFPImpl(const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const EZHRegisterInfo *RegInfo =
      static_cast<const EZHRegisterInfo *>(STI.getRegisterInfo());
  if (MF.getTarget().Options.DisableFramePointerElim(MF))
    return true;
  return MFI.hasVarSizedObjects() || MFI.isFrameAddressTaken() ||
         RegInfo->hasStackRealignment(MF);
}

bool EZHFrameLowering::assignCalleeSavedSpillSlots(
    MachineFunction &MF, const TargetRegisterInfo *TRI,
    std::vector<CalleeSavedInfo> &CSI) const {
  if (CSI.empty())
    return true;

  MachineFrameInfo &MFI = MF.getFrameInfo();
  EZHMachineFunctionInfo *FuncInfo = MF.getInfo<EZHMachineFunctionInfo>();
  const TargetRegisterInfo *RegInfo = MF.getSubtarget().getRegisterInfo();

  unsigned VarArgsSaveSize = FuncInfo->getVarArgsSaveSize();
  unsigned CSRSize = CSI.size() * 4;

  MFI.setStackSize(MFI.getStackSize() + CSRSize + VarArgsSaveSize);

  int64_t Offset = -static_cast<int64_t>(VarArgsSaveSize);

  for (auto &CS : CSI) {
    MCRegister Reg = CS.getReg();
    const TargetRegisterClass *RC = RegInfo->getMinimalPhysRegClass(Reg);
    unsigned Size = RegInfo->getSpillSize(*RC);

    Offset -= Size;

    int FrameIdx = MFI.CreateFixedSpillStackObject(Size, Offset);
    assert(FrameIdx < 0 && "Fixed stack object must have negative index!");
    CS.setFrameIdx(FrameIdx);
  }

  return true;
}

void EZHFrameLowering::determineCalleeSaves(MachineFunction &MF,
                                            BitVector &SavedRegs,
                                            RegScavenger *RS) const {
  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);

  if (hasFP(MF)) {
    SavedRegs.set(EZH::R7);
  }

  if (STI.hasBitSliceInterrupts() || MF.getFrameInfo().hasCalls()) {
    SavedRegs.set(EZH::RA);
  }

  const EZHRegisterInfo *RegInfo =
      static_cast<const EZHRegisterInfo *>(STI.getRegisterInfo());
  if (RegInfo->hasBasePointer(MF)) {
    SavedRegs.set(RegInfo->getBaseRegister());
  }

  MachineFrameInfo &MFI = MF.getFrameInfo();
  if (RS) {
    // The scavenging slot is only needed when eliminateFrameIndex may face an
    // offset outside the memory-op immediate ranges (word [-512,508], byte
    // [-128,127]) and must materialize the address in a scratch register.
    // Small frames -- the common case on a 32KB-SRAM part -- can never need
    // it; skipping the slot lets zero-local leaf functions drop their frame
    // entirely. 32 bytes of headroom covers the callee-saved spills that are
    // not yet part of the estimate; the 120 threshold keeps the worst case
    // safely inside the tightest (byte) range.
    if (MFI.estimateStackSize(MF) + 32 >= 120 || MFI.hasVarSizedObjects()) {
      const TargetRegisterClass &RC = EZH::GPRRegClass;
      unsigned Size = STI.getRegisterInfo()->getSpillSize(RC);
      Align Alignment = STI.getRegisterInfo()->getSpillAlign(RC);

      int FI = MFI.CreateSpillStackObject(Size, Alignment);
      RS->addScavengingFrameIndex(FI);
    }
  }
}

void EZHFrameLowering::processFunctionBeforeFrameFinalized(
    MachineFunction &MF, RegScavenger *RS) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  for (int i = 0, e = MFI.getObjectIndexEnd(); i != e; ++i) {
    if (MFI.isDeadObjectIndex(i))
      continue;
    Align Alignment = MFI.getObjectAlign(i);
    if (Alignment < Align(4)) {
      MFI.setObjectAlignment(i, Align(4));
    }
  }
}
