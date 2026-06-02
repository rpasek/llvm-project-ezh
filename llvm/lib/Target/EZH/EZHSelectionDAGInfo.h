//===-- EZHSelectionDAGInfo.h - EZH SelectionDAG Info ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the EZH subclass for SelectionDAGTargetInfo.
//
// Description:
//   Declares EZHSelectionDAGInfo, providing target-specific hooks for
//   SelectionDAG memory operations (memcpy, memset).
//
// Copied From:
//   Lanai target backend (llvm/lib/Target/Lanai/LanaiSelectionDAGInfo.h).
//
// Changes:
//   Renamed LanaiSelectionDAGInfo to EZHSelectionDAGInfo; adapted target hooks
//   for EZH.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EZH_EZHSELECTIONDAGINFO_H
#define LLVM_LIB_TARGET_EZH_EZHSELECTIONDAGINFO_H

#include "llvm/CodeGen/SelectionDAGTargetInfo.h"

namespace llvm {

class EZHSelectionDAGInfo : public SelectionDAGTargetInfo {
public:
  EZHSelectionDAGInfo();

  bool isTargetMemoryOpcode(unsigned Opcode) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EZH_EZHSELECTIONDAGINFO_H
