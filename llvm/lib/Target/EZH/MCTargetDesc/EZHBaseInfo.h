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
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EZH_MCTARGETDESC_EZHBASEINFO_H
#define LLVM_LIB_TARGET_EZH_MCTARGETDESC_EZHBASEINFO_H

#include "EZHMCTargetDesc.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/ErrorHandling.h"

namespace llvm {

// EZHII - This namespace holds all of the target specific flags that
// instruction info tracks.
namespace EZHII {
// Target Operand Flag enum.
enum TOF {
  //===------------------------------------------------------------------===//
  // EZH Specific MachineOperand flags.
  MO_NO_FLAG,

  // MO_ABS_HI/LO - Represents the hi or low part of an absolute symbol
  // address.
  MO_ABS_HI,
  MO_ABS_LO,
};
} // namespace EZHII

static inline unsigned getEZHRegisterNumbering(MCRegister Reg) {
  switch (Reg.id()) {
  case EZH::R0:
    return 0;
  case EZH::R1:
    return 1;
  case EZH::R2:
  case EZH::PC:
    return 2;
  case EZH::R3:
    return 3;
  case EZH::R4:
  case EZH::SP:
    return 4;
  case EZH::R5:
  case EZH::FP:
    return 5;
  case EZH::R6:
    return 6;
  case EZH::R7:
    return 7;
  case EZH::R8:
  case EZH::RV:
    return 8;
  case EZH::R9:
    return 9;
  case EZH::R10:
  case EZH::RR1:
    return 10;
  case EZH::R11:
  case EZH::RR2:
    return 11;
  case EZH::R12:
    return 12;
  case EZH::R13:
    return 13;
  case EZH::R14:
    return 14;
  case EZH::R15:
  case EZH::RCA:
    return 15;
  case EZH::R16:
    return 16;
  case EZH::R17:
    return 17;
  case EZH::R18:
    return 18;
  case EZH::R19:
    return 19;
  case EZH::R20:
    return 20;
  case EZH::R21:
    return 21;
  case EZH::R22:
    return 22;
  case EZH::R23:
    return 23;
  case EZH::R24:
    return 24;
  case EZH::R25:
    return 25;
  case EZH::R26:
    return 26;
  case EZH::R27:
    return 27;
  case EZH::R28:
    return 28;
  case EZH::R29:
    return 29;
  case EZH::R30:
    return 30;
  case EZH::R31:
    return 31;
  default:
    llvm_unreachable("Unknown register number!");
  }
}
} // namespace llvm
#endif // LLVM_LIB_TARGET_EZH_MCTARGETDESC_EZHBASEINFO_H
