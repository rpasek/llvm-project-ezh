//===-- EZHBaseInfo.h - Top level definitions for EZH MC ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains small standalone helper functions and enum definitions for
// the EZH target useful for the compiler back-end and the MC libraries.
//
// Description:
//   Defines MC-level bitfield masks, instruction format flags, and register
//   encodings shared across CodeGen, AsmParser, and Disassembler.
//
// Copied From:
//   Lanai target backend (llvm/lib/Target/Lanai/MCTargetDesc/LanaiBaseInfo.h).
//
// Changes:
//   Replaced Lanai instruction flags with EZH-specific instruction flag bits
//   and operand type enumeration constants.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EZH_MCTARGETDESC_EZHBASEINFO_H
#define LLVM_LIB_TARGET_EZH_MCTARGETDESC_EZHBASEINFO_H

#include "EZHMCTargetDesc.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/Support/ErrorHandling.h"
#include <cstdint>

namespace llvm {

namespace EZHII {

// Instruction TSFlags bits
enum {
  IsWordMem = 1 << 0,
  IsPredicated = 1 << 1,
  IsPredicable = 1 << 2,
};
} // namespace EZHII

// The per_read/per_write instructions reach memory-mapped peripherals through a
// 20-bit offset added to a fixed base. That base is the Cortex-M architectural
// peripheral region (0x40000000): the EZH/SmartDMA core shares the system bus,
// so this is the same on every SmartDMA part -- it is NOT chip-specific. The
// 1 MB span is the reach of the 20-bit offset. CodeGen folds an aligned
// constant store/load in this window into per_write/per_read; the MC layer
// converts between the physical address and the encoded offset.
constexpr uint32_t EZHPeripheralBase = 0x40000000;
constexpr uint32_t EZHPeripheralSize = 0x00100000;  // 1 MB == 20-bit offset window
constexpr uint32_t EZHPeripheralEnd = EZHPeripheralBase + EZHPeripheralSize - 1;

} // namespace llvm
#endif // LLVM_LIB_TARGET_EZH_MCTARGETDESC_EZHBASEINFO_H
