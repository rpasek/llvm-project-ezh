//===-- EZHRegisterInfo.cpp - EZH Register Information ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the EZH implementation of the TargetRegisterInfo class.
//
// Description:
//   Implements physical register management, reserved register tracking
//   (reserving core system registers SP, PC, RA), and eliminateFrameIndex for
//   stack offset resolution.
//
// Copied From:
//   Lanai target backend (llvm/lib/Target/Lanai/LanaiRegisterInfo.cpp).
//
// Changes:
//   Implemented getReservedRegs reserving internal core system registers SP,
//   PC, and RA; implemented eliminateFrameIndex converting frame indices into
//   base register plus immediate offset loads/stores.
//
//===----------------------------------------------------------------------===//

#include "EZHRegisterInfo.h"
#include "EZHCondCode.h"
#include "EZHFrameLowering.h"
#include "EZHInstrInfo.h"
#include "EZHMachineFunctionInfo.h"
#include "MCTargetDesc/EZHBaseInfo.h"
#include "MCTargetDesc/EZHMCTargetDesc.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"

#define GET_REGINFO_TARGET_DESC
#include "EZHGenRegisterInfo.inc"

using namespace llvm;

EZHRegisterInfo::EZHRegisterInfo() : EZHGenRegisterInfo(EZH::RA) {}

const uint16_t *
EZHRegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  return CSR_EZH_SaveList;
}

BitVector EZHRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());

  auto ReserveRegAndAliases = [&](Register Reg) {
    for (MCRegAliasIterator Alias(Reg, this, true); Alias.isValid(); ++Alias)
      Reserved.set(*Alias);
  };

  ReserveRegAndAliases(EZH::SP);
  ReserveRegAndAliases(EZH::PC);
  ReserveRegAndAliases(EZH::RA);
  ReserveRegAndAliases(EZH::GPO);
  ReserveRegAndAliases(EZH::GPD);
  ReserveRegAndAliases(EZH::CFS);
  ReserveRegAndAliases(EZH::CFM);
  ReserveRegAndAliases(EZH::GPI);

  const TargetFrameLowering *TFI = MF.getSubtarget().getFrameLowering();
  if (TFI->hasFP(MF))
    ReserveRegAndAliases(EZH::R7);

  if (hasBasePointer(MF))
    ReserveRegAndAliases(getBaseRegister());

  return Reserved;
}

bool EZHRegisterInfo::requiresRegisterScavenging(
    const MachineFunction &MF) const {
  return true;
}

bool EZHRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                          int SPAdj, unsigned FIOperandNum,
                                          RegScavenger *RS) const {
  MachineInstr &MI = *II;
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  const EZHInstrInfo *TII =
      static_cast<const EZHInstrInfo *>(MF.getSubtarget().getInstrInfo());
  DebugLoc DL = MI.getDebugLoc();

  int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
  int Offset = MI.getOperand(FIOperandNum + 1).getImm();

  // The memory-form tail call executes after the epilogue has torn the
  // frame down: SP is back at its entry value, so the slot sits at its raw
  // (negative) object offset from SP -- just below the final stack pointer.
  // The callee's own pushes land on that memory only after this load has
  // consumed it. The slot is a fixed object pinned at the top of the frame
  // (LowerCall places it just below the vararg save area), so this offset
  // is a small constant regardless of the size of the locals; the range
  // check is a safety net that cannot fire for compiler-generated frames.
  if (MI.getOpcode() == EZH::TCRETURN_MEM ||
      MI.getOpcode() == EZH::TCRETURN_MEM_CC) {
    int PostOffset = MF.getFrameInfo().getObjectOffset(FrameIndex) + Offset;
    if (PostOffset < -512 || PostOffset > 508 || (PostOffset & 3) != 0)
      report_fatal_error(
          "EZH musttail: tail-call target slot out of load range");
    MI.getOperand(FIOperandNum).ChangeToRegister(EZH::SP, false);
    MI.getOperand(FIOperandNum + 1).ChangeToImmediate(PostOffset);
    return false;
  }

  const TargetFrameLowering *TFI = MF.getSubtarget().getFrameLowering();
  Register FrameReg = EZH::SP;

  bool HasFP = TFI->hasFP(MF);
  bool HasBP = hasBasePointer(MF);
  bool IsFixed = MF.getFrameInfo().isFixedObjectIndex(FrameIndex);

  if (HasBP && !IsFixed) {
    FrameReg = getBaseRegister(); // R6
    Offset += MF.getFrameInfo().getObjectOffset(FrameIndex);
    Offset += MF.getFrameInfo().getStackSize() + SPAdj;
  } else if (HasFP) {
    FrameReg = EZH::R7;
    const std::vector<CalleeSavedInfo> &CSI =
        MF.getFrameInfo().getCalleeSavedInfo();
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
    Offset += MF.getFrameInfo().getObjectOffset(FrameIndex) -
              MF.getFrameInfo().getObjectOffset(FPFI);
  } else {
    Offset += MF.getFrameInfo().getObjectOffset(FrameIndex);
    Offset += MF.getFrameInfo().getStackSize() + SPAdj;
  }

  // Determine if the offset fits in the immediate field of the memory
  // instruction.
  unsigned Opc = MI.getOpcode();
  const MCInstrDesc &Desc = TII->get(Opc);
  bool IsWordMem = (Desc.TSFlags & EZHII::IsWordMem);

  bool Fits = false;
  if (IsWordMem) {
    if (Offset >= -512 && Offset <= 508 && (Offset & 3) == 0)
      Fits = true;
  } else {
    if (Offset >= -128 && Offset <= 127)
      Fits = true;
  }

  if (Fits) {
    MI.getOperand(FIOperandNum).ChangeToRegister(FrameReg, false);
    MI.getOperand(FIOperandNum + 1).ChangeToImmediate(Offset);
    return false;
  }

  // Offset does not fit natively. Use the RegScavenger to materialize it.
  Register ScratchReg;
  bool PushPopFallback = false;

  if (RS) {
    ScratchReg = RS->FindUnusedReg(&EZH::GPRRegClass);
    // Ensure the scavenged scratch register is not actively used by the memory
    // instruction itself!
    if (ScratchReg && (MI.readsRegister(ScratchReg, this) ||
                       MI.definesRegister(ScratchReg, this))) {
      ScratchReg = EZH::NoRegister;
    }
    // Bypass scavengeRegisterBackwards to avoid risky spills that cause
    // recursion crashes. If no free register is found, we safe-fallback to
    // pushing/popping a temporary register.
  }

  if (!ScratchReg) {
    Register FallbackReg = EZH::NoRegister;
    static const Register Candidates[] = {EZH::R4, EZH::R5, EZH::R6, EZH::R7};
    for (Register Reg : Candidates) {
      if (Reg != FrameReg && !MI.readsRegister(Reg, this) &&
          !MI.definesRegister(Reg, this)) {
        FallbackReg = Reg;
        break;
      }
    }
    if (!FallbackReg) {
      static const Register Callers[] = {EZH::R0, EZH::R1, EZH::R2, EZH::R3};
      for (Register Reg : Callers) {
        if (Reg != FrameReg && !MI.readsRegister(Reg, this) &&
            !MI.definesRegister(Reg, this)) {
          FallbackReg = Reg;
          break;
        }
      }
    }
    assert(FallbackReg != EZH::NoRegister &&
           "Could not find any fallback register!");
    ScratchReg = FallbackReg;

    // Only preserve the borrowed register if its current value is live past
    // this instruction. If it is dead, we can clobber it as scratch directly:
    // pushing/popping a dead register wastes two instructions, and marking the
    // save as reading an undef value (to satisfy -verify-machineinstrs) instead
    // lets a later pass delete the save/restore pair entirely -- which silently
    // corrupts the value when the register is in fact live. Compute liveness at
    // MI and decide.
    LivePhysRegs LiveRegs(*this);
    LiveRegs.addLiveOuts(MBB);
    for (MachineInstr &Below : llvm::reverse(MBB)) {
      if (&Below == &MI)
        break;
      LiveRegs.stepBackward(Below);
    }
    PushPopFallback = LiveRegs.contains(ScratchReg);
  }

  if (PushPopFallback) {
    if (FrameReg == EZH::SP)
      Offset += 4; // Compensate for SP decrement due to push when SP is base
    // ScratchReg is live here, so its value has a reaching definition: read it
    // normally (no undef) and let the matching pop below restore it.
    BuildMI(MBB, II, DL, TII->get(EZH::STR_PRE), EZH::SP)
        .addReg(ScratchReg)
        .addReg(EZH::SP)
        .addImm(-4)
        .addImm(EZHCC::ICC_EU);
  }

  if (Offset > 0 && Offset <= 2047) {
    BuildMI(MBB, II, DL, TII->get(EZH::ADD_IMM))
        .addDef(ScratchReg)
        .addReg(FrameReg)
        .addImm(Offset)
        .addImm(EZHCC::ICC_EU);
  } else if (Offset < 0 && Offset >= -2048) {
    BuildMI(MBB, II, DL, TII->get(EZH::SUB_IMM))
        .addDef(ScratchReg)
        .addReg(FrameReg)
        .addImm(-Offset)
        .addImm(EZHCC::ICC_EU);
  } else {
    // Load Offset into ScratchReg, then add SP
    uint32_t UOffset = static_cast<uint32_t>(Offset);
    uint32_t ShiftAmt = 0;
    while ((UOffset & 1) == 0 && ShiftAmt < 31 && UOffset != 0) {
      UOffset >>= 1;
      ++ShiftAmt;
    }
    if (UOffset < 1024) {
      BuildMI(MBB, II, DL, TII->get(EZH::LOAD_SIMM))
          .addDef(ScratchReg)
          .addImm(UOffset)
          .addImm(ShiftAmt)
          .addImm(EZHCC::ICC_EU);
    } else {
      // Process strictly from High-to-Low (MSB to LSB)
      // This allows us to load the highest non-zero byte chunk natively via
      // MOVri (or MOVSri), left-shift the scratch register by 8 bits, and
      // directly OR the next lower-order non-zero byte chunk via ORri.
      // This keeps code size minimal and avoids secondary scratch registers.
      uint32_t UOffset = static_cast<uint32_t>(Offset);

      // Extract non-zero byte chunks
      uint8_t Bytes[4];
      Bytes[3] = (UOffset >> 24) & 0xFF;
      Bytes[2] = (UOffset >> 16) & 0xFF;
      Bytes[1] = (UOffset >> 8) & 0xFF;
      Bytes[0] = UOffset & 0xFF;

      int FirstNonZeroIdx = -1;
      for (int i = 3; i >= 0; --i) {
        if (Bytes[i] != 0) {
          FirstNonZeroIdx = i;
          break;
        }
      }

      if (FirstNonZeroIdx != -1) {
        int NextNonZeroIdx = -1;
        for (int i = FirstNonZeroIdx - 1; i >= 0; --i) {
          if (Bytes[i] != 0) {
            NextNonZeroIdx = i;
            break;
          }
        }

        if (NextNonZeroIdx != -1) {
          unsigned FirstShift = (FirstNonZeroIdx - NextNonZeroIdx) * 8;
          BuildMI(MBB, II, DL, TII->get(EZH::LOAD_SIMM), ScratchReg)
              .addImm(Bytes[FirstNonZeroIdx])
              .addImm(FirstShift)
              .addImm(EZHCC::ICC_EU);

          BuildMI(MBB, II, DL, TII->get(EZH::OR_IMM), ScratchReg)
              .addReg(ScratchReg)
              .addImm(Bytes[NextNonZeroIdx])
              .addImm(EZHCC::ICC_EU);

          unsigned ShiftAccum = 0;
          for (int i = NextNonZeroIdx - 1; i >= 0; --i) {
            ShiftAccum += 8;
            if (Bytes[i] != 0) {
              BuildMI(MBB, II, DL, TII->get(EZH::LSL), ScratchReg)
                  .addReg(ScratchReg)
                  .addImm(ShiftAccum)
                  .addImm(EZHCC::ICC_EU);
              ShiftAccum = 0;

              BuildMI(MBB, II, DL, TII->get(EZH::OR_IMM), ScratchReg)
                  .addReg(ScratchReg)
                  .addImm(Bytes[i])
                  .addImm(EZHCC::ICC_EU);
            }
          }
          if (ShiftAccum > 0) {
            BuildMI(MBB, II, DL, TII->get(EZH::LSL), ScratchReg)
                .addReg(ScratchReg)
                .addImm(ShiftAccum)
                .addImm(EZHCC::ICC_EU);
          }
        } else {
          BuildMI(MBB, II, DL, TII->get(EZH::LOAD_IMM), ScratchReg)
              .addImm(Bytes[FirstNonZeroIdx])
              .addImm(EZHCC::ICC_EU);
          if (FirstNonZeroIdx > 0) {
            BuildMI(MBB, II, DL, TII->get(EZH::LSL), ScratchReg)
                .addReg(ScratchReg)
                .addImm(FirstNonZeroIdx * 8)
                .addImm(EZHCC::ICC_EU);
          }
        }
      }
    }
    BuildMI(MBB, II, DL, TII->get(EZH::ADD))
        .addDef(ScratchReg)
        .addReg(ScratchReg)
        .addReg(FrameReg)
        .addImm(EZHCC::ICC_EU);
  }

  MI.getOperand(FIOperandNum).ChangeToRegister(ScratchReg, false);
  MI.getOperand(FIOperandNum + 1).ChangeToImmediate(0);

  if (PushPopFallback) {
    MachineBasicBlock::iterator NextII = std::next(II);
    BuildMI(MBB, NextII, DL, TII->get(EZH::LDR_POST), ScratchReg)
        .addReg(EZH::SP, RegState::Define)
        .addReg(EZH::SP)
        .addImm(4)
        .addImm(EZHCC::ICC_EU);
  }

  return false;
}

Register EZHRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  const TargetFrameLowering *TFI = MF.getSubtarget().getFrameLowering();
  return TFI->hasFP(MF) ? EZH::R7 : EZH::SP;
}

const uint32_t *
EZHRegisterInfo::getCallPreservedMask(const MachineFunction &MF,
                                      CallingConv::ID CC) const {
  return CSR_EZH_RegMask;
}

unsigned EZHRegisterInfo::getRARegister() const { return EZH::RA; }

Register EZHRegisterInfo::getBaseRegister() const { return EZH::R6; }

bool EZHRegisterInfo::hasBasePointer(const MachineFunction &MF) const {
  return hasStackRealignment(MF);
}

bool EZHRegisterInfo::shouldCoalesce(
    MachineInstr *MI, const TargetRegisterClass *SrcRC, unsigned SubReg,
    const TargetRegisterClass *DstRC, unsigned DstSubReg,
    const TargetRegisterClass *NewRC, LiveIntervals &LIS) const {
  // Protect stack pointer allocations, saves, and restores from aggressive
  // register coalescing.
  for (const MachineOperand &MO : MI->operands()) {
    if (MO.isReg() && MO.getReg() == EZH::SP)
      return false;
  }

  return true;
}

bool EZHRegisterInfo::isAsmClobberable(const MachineFunction &MF,
                                       MCRegister PhysReg) const {
  return !getReservedRegs(MF).test(PhysReg);
}

bool EZHRegisterInfo::isInlineAsmReadOnlyReg(const MachineFunction &MF,
                                             MCRegister PhysReg) const {
  return false;
}
