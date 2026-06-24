//===-- EZHSelectionDAGInfo.h - EZH SelectionDAG Info -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the EZH subclass for TargetSelectionDAGInfo.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EZH_EZHSELECTIONDAGINFO_H
#define LLVM_LIB_TARGET_EZH_EZHSELECTIONDAGINFO_H

#include "llvm/CodeGen/SelectionDAGTargetInfo.h"
#include "llvm/Target/TargetMachine.h"

#define GET_SDNODE_ENUM
#include "EZHGenSDNodeInfo.inc"

namespace llvm {

class EZHSelectionDAGInfo : public SelectionDAGGenTargetInfo {
public:
  EZHSelectionDAGInfo();

  SDValue EmitTargetCodeForMemcpy(SelectionDAG &DAG, const SDLoc &dl,
                                  SDValue Chain, SDValue Dst, SDValue Src,
                                  SDValue Size, Align DstAlign, Align SrcAlign,
                                  bool isVolatile, bool AlwaysInline,
                                  MachinePointerInfo DstPtrInfo,
                                  MachinePointerInfo SrcPtrInfo) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EZH_EZHSELECTIONDAGINFO_H
