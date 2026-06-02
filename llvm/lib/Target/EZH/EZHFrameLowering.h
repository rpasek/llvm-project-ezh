//===-- EZHFrameLowering.h - Define frame lowering for EZH --*- C++-*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This class implements EZH-specific bits of TargetFrameLowering class.
//
// Description:
//   Declares EZHFrameLowering, defining stack frame layout, alignment
//   requirements, and function prologue/epilogue emission interfaces.
//
// Copied From:
//   Lanai target backend (llvm/lib/Target/Lanai/LanaiFrameLowering.h).
//
// Changes:
//   Renamed LanaiFrameLowering to EZHFrameLowering; configured EZH stack
//   alignment and growth direction; declared custom prologue and epilogue
//   insertion methods.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EZH_EZHFRAMELOWERING_H
#define LLVM_LIB_TARGET_EZH_EZHFRAMELOWERING_H

#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {

class BitVector;
class EZHSubtarget;

class EZHFrameLowering : public TargetFrameLowering {
protected:
  const EZHSubtarget &STI;

public:
  explicit EZHFrameLowering(const EZHSubtarget &Subtarget)
      : TargetFrameLowering(TargetFrameLowering::StackGrowsDown,
                            /*StackAlignment=*/Align(4),
                            /*LocalAreaOffset=*/0),
        STI(Subtarget) {}

  bool hasReservedCallFrame(const MachineFunction &MF) const override;

  // emitProlog/emitEpilog - These methods insert prolog and epilog code into
  // the function.
  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

  bool spillCalleeSavedRegisters(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MI,
                                 ArrayRef<CalleeSavedInfo> CSI,
                                 const TargetRegisterInfo *TRI) const override;

  bool
  restoreCalleeSavedRegisters(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MI,
                              MutableArrayRef<CalleeSavedInfo> CSI,
                              const TargetRegisterInfo *TRI) const override;

  MachineBasicBlock::iterator
  eliminateCallFramePseudoInstr(MachineFunction &MF, MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator I) const override;

  bool
  assignCalleeSavedSpillSlots(MachineFunction &MF,
                              const TargetRegisterInfo *TRI,
                              std::vector<CalleeSavedInfo> &CSI) const override;

  void determineCalleeSaves(MachineFunction &MF, BitVector &SavedRegs,
                            RegScavenger *RS = nullptr) const override;

  void processFunctionBeforeFrameFinalized(
      MachineFunction &MF, RegScavenger *RS = nullptr) const override;

protected:
  bool hasFPImpl(const MachineFunction &MF) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EZH_EZHFRAMELOWERING_H
