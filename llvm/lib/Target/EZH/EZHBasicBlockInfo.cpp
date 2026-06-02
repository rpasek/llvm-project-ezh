//===--- EZHBasicBlockInfo.cpp - Utilities for block sizes ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Description:
//   Implements basic block size and offset calculation utilities used by the
//   EZH Constant Islands pass.
//
// Copied From:
//   ARM target backend (llvm/lib/Target/ARM/ARMBasicBlockInfo.cpp).
//
// Changes:
//   Updated instruction byte size calculations to account for EZH 32-bit
//   instruction encodings and preceding condition codes.
//
//===----------------------------------------------------------------------===//

#include "EZHBasicBlockInfo.h"
#include "EZH.h"
#include "EZHInstrInfo.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "ezh-bb-utils"

using namespace llvm;

namespace llvm {

void EZHBasicBlockUtils::computeBlockSize(MachineBasicBlock *MBB) {
  BasicBlockInfo &BBI = BBInfo[MBB->getNumber()];
  BBI.Size = 0;
  BBI.PostAlign = Align(1);

  for (MachineInstr &I : *MBB) {
    BBI.Size += TII->getInstSizeInBytes(I);
  }
}

/// getOffsetOf - Return the current offset of the specified machine instruction
/// from the start of the function.  This offset changes as stuff is moved
/// around inside the function.
unsigned EZHBasicBlockUtils::getOffsetOf(MachineInstr *MI) const {
  const MachineBasicBlock *MBB = MI->getParent();

  // The offset is composed of two things: the sum of the sizes of all MBB's
  // before this instruction's block, and the offset from the start of the block
  // it is in.
  unsigned Offset = BBInfo[MBB->getNumber()].Offset;

  bool Found = false;
  for (const MachineInstr &I : *MBB) {
    if (&I == MI) {
      Found = true;
      break;
    }
    Offset += TII->getInstSizeInBytes(I);
  }
  assert(Found && "Didn't find MI in its own basic block?");
  return Offset;
}

/// isBBInRange - Returns true if the distance between specific MI and
/// specific BB can fit in MI's displacement field.
bool EZHBasicBlockUtils::isBBInRange(MachineInstr *MI,
                                     MachineBasicBlock *DestBB,
                                     unsigned MaxDisp) const {
  unsigned PCAdj = 4;
  unsigned BrOffset = getOffsetOf(MI) + PCAdj;
  unsigned DestOffset = BBInfo[DestBB->getNumber()].Offset;

  LLVM_DEBUG(dbgs() << "Branch of destination " << printMBBReference(*DestBB)
                    << " from " << printMBBReference(*MI->getParent())
                    << " max delta=" << MaxDisp << " from " << getOffsetOf(MI)
                    << " to " << DestOffset << " offset "
                    << int(DestOffset - BrOffset) << "\t" << *MI);

  if (BrOffset <= DestOffset) {
    // Branch before the Dest.
    if (DestOffset - BrOffset <= MaxDisp)
      return true;
  } else {
    if (BrOffset - DestOffset <= MaxDisp)
      return true;
  }
  return false;
}

void EZHBasicBlockUtils::adjustBBOffsetsAfter(MachineBasicBlock *BB) {
  assert(BB->getParent() == &MF &&
         "Basic block is not a child of the current function.\n");

  unsigned BBNum = BB->getNumber();
  LLVM_DEBUG(dbgs() << "Adjust block:\n"
                    << " - name: " << BB->getName() << "\n"
                    << " - number: " << BB->getNumber() << "\n"
                    << " - function: " << MF.getName() << "\n"
                    << "   - blocks: " << MF.getNumBlockIDs() << "\n");

  for (unsigned i = BBNum + 1, e = MF.getNumBlockIDs(); i < e; ++i) {
    // Get the offset at the end of the layout predecessor.
    // Include the alignment of the current block.
    const Align Align = MF.getBlockNumbered(i)->getAlignment();
    const unsigned Offset = BBInfo[i - 1].postOffset(Align);

    // This is where block i begins.  Stop if the offset is already correct,
    // and we have updated 2 blocks.  This is the maximum number of blocks
    // changed before calling this function.
    if (i > BBNum + 2 && BBInfo[i].Offset == Offset)
      break;

    BBInfo[i].Offset = Offset;
  }
}

} // end namespace llvm
