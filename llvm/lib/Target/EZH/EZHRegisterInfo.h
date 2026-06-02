//===- EZHRegisterInfo.h - EZH Register Information Impl ----*- C++ -*-===//
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
//   Declares EZHRegisterInfo, managing physical register definitions, register
//   classes, calling convention register allocation, and stack frame index
//   elimination.
//
// Copied From:
//   Lanai target backend (llvm/lib/Target/Lanai/LanaiRegisterInfo.h).
//
// Changes:
//   Adapted to EZH register hierarchy (GPRs r0-r7, gp0-gp7, system registers
//   sp, pc, ra); declared frame index elimination and reserved register
//   tracking.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EZH_EZHREGISTERINFO_H
#define LLVM_LIB_TARGET_EZH_EZHREGISTERINFO_H

#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/Register.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

#define GET_REGINFO_HEADER
#include "EZHGenRegisterInfo.inc"

namespace llvm {

class LiveIntervals;
class RegScavenger;

/// EZH Register Information.
struct EZHRegisterInfo : public EZHGenRegisterInfo {
  EZHRegisterInfo();

  const uint32_t *getCallPreservedMask(const MachineFunction &MF,
                                       CallingConv::ID) const override;

  // Code Generation virtual methods.
  const uint16_t *
  getCalleeSavedRegs(const MachineFunction *MF = nullptr) const override;

  BitVector getReservedRegs(const MachineFunction &MF) const override;

  bool requiresRegisterScavenging(const MachineFunction &MF) const override;

  bool eliminateFrameIndex(MachineBasicBlock::iterator II, int SPAdj,
                           unsigned FIOperandNum,
                           RegScavenger *RS = nullptr) const override;

  // Debug information queries.
  unsigned getRARegister() const;
  Register getFrameRegister(const MachineFunction &MF) const override;
  Register getBaseRegister() const;
  bool hasBasePointer(const MachineFunction &MF) const;
  bool shouldCoalesce(MachineInstr *MI, const TargetRegisterClass *SrcRC,
                      unsigned SubReg, const TargetRegisterClass *DstRC,
                      unsigned DstSubReg, const TargetRegisterClass *NewRC,
                      LiveIntervals &LIS) const override;

  bool isAsmClobberable(const MachineFunction &MF,
                        MCRegister PhysReg) const override;
  bool isInlineAsmReadOnlyReg(const MachineFunction &MF,
                              MCRegister PhysReg) const override;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EZH_EZHREGISTERINFO_H
