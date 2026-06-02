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
// Description:
//   Implements target hooks for SelectionDAG memory routines.
//
// Copied From:
//   Lanai target backend (llvm/lib/Target/Lanai/LanaiSelectionDAGInfo.cpp).
//
// Changes:
//   Minimal implementation stub adapted for EZH memory characteristics.
//
//===----------------------------------------------------------------------===//

#include "EZHSelectionDAGInfo.h"
#include "EZHISelLowering.h"

#define DEBUG_TYPE "ezh-selectiondag-info"

using namespace llvm;

EZHSelectionDAGInfo::EZHSelectionDAGInfo() : SelectionDAGTargetInfo() {}

bool EZHSelectionDAGInfo::isTargetMemoryOpcode(unsigned Opcode) const {
  return Opcode == EZHISD::PER_READ || Opcode == EZHISD::PER_WRITE;
}
