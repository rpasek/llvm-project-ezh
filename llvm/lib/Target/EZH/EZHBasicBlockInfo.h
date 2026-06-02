//===-- EZHBasicBlockInfo.h - Basic Block Information -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Description:
//   Defines data structures (BasicBlockInfo) for tracking basic block size,
//   alignment, and post-layout byte offsets during Constant Island processing.
//
// Copied From:
//   ARM target backend (llvm/lib/Target/ARM/ARMBasicBlockInfo.h).
//
// Changes:
//   Adapted struct definitions and member utilities to track 32-bit EZH
//   instruction lengths and basic block byte offsets.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EZH_EZHBASICBLOCKINFO_H
#define LLVM_LIB_TARGET_EZH_EZHBASICBLOCKINFO_H

#include "EZHInstrInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/Support/MathExtras.h"
#include <algorithm>
#include <cstdint>

namespace llvm {

struct BasicBlockInfo;
using BBInfoVector = SmallVectorImpl<BasicBlockInfo>;

/// BasicBlockInfo - Information about the offset and size of a single
/// basic block.
struct BasicBlockInfo {
  /// Offset - Distance from the beginning of the function to the beginning
  /// of this basic block.
  ///
  /// Offsets are computed assuming alignment padding before an aligned
  /// block.
  unsigned Offset = 0;

  /// Size - Size of the basic block in bytes.  If the block contains
  /// inline assembly, this is a worst case estimate.
  ///
  /// The size does not include any alignment padding whether from the
  /// beginning of the block, or from an aligned jump table at the end.
  unsigned Size = 0;

  /// PostAlign - When > 1, the block terminator contains a .align
  /// directive, so the end of the block is aligned to PostAlign bytes.
  Align PostAlign;

  BasicBlockInfo() = default;

  /// Compute the offset immediately following this block.  If Align is
  /// specified, return the offset the successor block will get if it has
  /// this alignment.
  unsigned postOffset(Align Alignment = Align(1)) const {
    unsigned PO = Offset + Size;
    const Align PA = std::max(PostAlign, Alignment);
    return alignTo(PO, PA);
  }

  /// Compute the number of known low bits of postOffset.
  /// Also considers the alignment of the next block if specified.
  unsigned postKnownBits(Align Alignment = Align(1)) const {
    return Log2(std::max(PostAlign, Alignment));
  }
};

class EZHBasicBlockUtils {

private:
  MachineFunction &MF;
  const EZHInstrInfo *TII = nullptr;
  SmallVector<BasicBlockInfo, 8> BBInfo;

public:
  EZHBasicBlockUtils(MachineFunction &MF) : MF(MF) {
    TII = static_cast<const EZHInstrInfo *>(MF.getSubtarget().getInstrInfo());
  }

  void computeAllBlockSizes() {
    BBInfo.resize(MF.getNumBlockIDs());
    for (MachineBasicBlock &MBB : MF)
      computeBlockSize(&MBB);
  }

  void computeBlockSize(MachineBasicBlock *MBB);

  unsigned getOffsetOf(MachineInstr *MI) const;

  unsigned getOffsetOf(MachineBasicBlock *MBB) const {
    return BBInfo[MBB->getNumber()].Offset;
  }

  void adjustBBOffsetsAfter(MachineBasicBlock *MBB);

  void adjustBBSize(MachineBasicBlock *MBB, int Size) {
    BBInfo[MBB->getNumber()].Size += Size;
  }

  bool isBBInRange(MachineInstr *MI, MachineBasicBlock *DestBB,
                   unsigned MaxDisp) const;

  void insert(unsigned BBNum, BasicBlockInfo BBI) {
    BBInfo.insert(BBInfo.begin() + BBNum, BBI);
  }

  void clear() { BBInfo.clear(); }

  BBInfoVector &getBBInfo() { return BBInfo; }
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EZH_EZHBASICBLOCKINFO_H
