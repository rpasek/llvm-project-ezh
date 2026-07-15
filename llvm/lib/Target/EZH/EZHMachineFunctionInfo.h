//===- EZHMachineFunctionInfo.h - EZH machine func info -------*- C++ -*-==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares EZH-specific per-machine-function information.
//
// Description:
//   Declares EZHMachineFunctionInfo, tracking per-function state such as
//   constant pool entries, jump tables, and vararg save area offsets.
//
// Copied From:
//   Lanai target backend (llvm/lib/Target/Lanai/LanaiMachineFunctionInfo.h).
//
// Changes:
//   Renamed LanaiMachineFunctionInfo to EZHMachineFunctionInfo; customized
//   members to track EZH Constant Island state and vararg frame indices.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EZH_EZHMACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_EZH_EZHMACHINEFUNCTIONINFO_H

#include "EZHRegisterInfo.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Allocator.h"

namespace llvm {

// EZHMachineFunctionInfo - This class is derived from MachineFunction and
// contains private EZH target-specific information for each MachineFunction.
class EZHMachineFunctionInfo : public MachineFunctionInfo {
  virtual void anchor();

  // SRetReturnReg - EZH ABI require that sret lowering includes
  // returning the value of the returned struct in a register. This field
  // holds the virtual register into which the sret argument is passed.
  Register SRetReturnReg;

  // VarArgsFrameIndex - FrameIndex for start of varargs area.
  int VarArgsFrameIndex;

  // VarArgsSaveSize - Size of the varargs register save area.
  unsigned VarArgsSaveSize;

  // VarArgsRegIdx - The first register index that is spilled for varargs.
  unsigned VarArgsRegIdx;

  // ForwardedMustTailRegParms - In a vararg function containing a musttail
  // call, the entry values of the unnamed argument registers, captured so
  // the tail call can forward the ellipsis.
  SmallVector<ForwardedRegister, 4> ForwardedMustTailRegParms;

  // TailCallSlot - Frame index and size of the memory-form tail-call target
  // slot: a fixed object pinned at the very top of the frame (just below
  // the vararg save area), so its offset from the restored entry SP is
  // small and constant no matter how large the locals are.
  int TailCallSlotFI = 0;
  unsigned TailCallSlotSize = 0;

public:
  EZHMachineFunctionInfo(const Function &F, const TargetSubtargetInfo *STI)
      : VarArgsFrameIndex(0), VarArgsSaveSize(0), VarArgsRegIdx(0) {}
  MachineFunctionInfo *
  clone(BumpPtrAllocator &Allocator, MachineFunction &DestMF,
        const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &Src2DstMBB)
      const override;

  Register getSRetReturnReg() const { return SRetReturnReg; }
  void setSRetReturnReg(Register Reg) { SRetReturnReg = Reg; }

  int getVarArgsFrameIndex() const { return VarArgsFrameIndex; }
  void setVarArgsFrameIndex(int Index) { VarArgsFrameIndex = Index; }

  unsigned getVarArgsSaveSize() const { return VarArgsSaveSize; }
  void setVarArgsSaveSize(unsigned Size) { VarArgsSaveSize = Size; }

  unsigned getVarArgsRegIdx() const { return VarArgsRegIdx; }
  void setVarArgsRegIdx(unsigned Idx) { VarArgsRegIdx = Idx; }

  SmallVectorImpl<ForwardedRegister> &getForwardedMustTailRegParms() {
    return ForwardedMustTailRegParms;
  }
  const SmallVectorImpl<ForwardedRegister> &getForwardedMustTailRegParms() const {
    return ForwardedMustTailRegParms;
  }

  int getTailCallSlotFI() const { return TailCallSlotFI; }
  unsigned getTailCallSlotSize() const { return TailCallSlotSize; }
  void setTailCallSlot(int FI, unsigned Size) {
    TailCallSlotFI = FI;
    TailCallSlotSize = Size;
  }
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EZH_EZHMACHINEFUNCTIONINFO_H
