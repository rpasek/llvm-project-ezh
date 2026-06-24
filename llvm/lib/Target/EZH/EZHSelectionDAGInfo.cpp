//===-- EZHSelectionDAGInfo.cpp - EZH SelectionDAG Info -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the EZHSelectionDAGInfo class.
//
//===----------------------------------------------------------------------===//

#include "EZHSelectionDAGInfo.h"

#define GET_SDNODE_DESC
#include "EZHGenSDNodeInfo.inc"

#define DEBUG_TYPE "ezh-selectiondag-info"

using namespace llvm;

EZHSelectionDAGInfo::EZHSelectionDAGInfo()
    : SelectionDAGGenTargetInfo(EZHGenSDNodeInfo) {}

SDValue EZHSelectionDAGInfo::EmitTargetCodeForMemcpy(
    SelectionDAG & /*DAG*/, const SDLoc & /*dl*/, SDValue /*Chain*/,
    SDValue /*Dst*/, SDValue /*Src*/, SDValue Size, Align /*DstAlign*/,
    Align /*SrcAlign*/, bool /*isVolatile*/, bool /*AlwaysInline*/,
    MachinePointerInfo /*DstPtrInfo*/,
    MachinePointerInfo /*SrcPtrInfo*/) const {
  ConstantSDNode *ConstantSize = dyn_cast<ConstantSDNode>(Size);
  if (!ConstantSize)
    return SDValue();

  return SDValue();
}
