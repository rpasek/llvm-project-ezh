//===-- EZHCondCode.h - EZH Condition Code Enumeration --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Description:
//   Defines enumeration constants and conversion utilities for EZH hardware
//   condition codes (e.g., EQ, NE, CS, CC, e_goto_ca).
//
// Copied From:
//   Lanai target backend (llvm/lib/Target/Lanai/LanaiCondCode.h).
//
// Changes:
//   Replaced Lanai condition codes with EZH hardware condition codes;
//   customized string mappings to match EZH hardware ALU flags (where flag
//   behavior operates similarly to x86, such as CA for unsigned
//   overflow/borrow).
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EZH_EZHCONDCODE_H
#define LLVM_LIB_TARGET_EZH_EZHCONDCODE_H

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/ErrorHandling.h"

namespace llvm {
namespace EZHCC {
enum CondCode {
  ICC_EU = 0,   // Execute Unconditionally
  ICC_ZE = 1,   // Zero (EQ)
  ICC_NZ = 2,   // Not Zero (NE)
  ICC_PO = 3,   // Positive (PL/GE)
  ICC_NE = 4,   // Negative (MI)
  ICC_AZ = 5,   // Above zero (GT)
  ICC_ZB = 6,   // Zero or below (LE)
  ICC_CA = 7,   // Carry set (CS)
  ICC_NC = 8,   // Carry not set (CC)
  ICC_CZ = 9,   // Carry set and zero
  ICC_SPO = 10, // Shift-only-when-Positive
  ICC_SNE = 11, // Shift-only-when-Negative
  ICC_NBS = 12, // Not Boolean-expression set
  ICC_NEX = 13, // External flag is not set
  ICC_BS = 14,  // Boolean-expression set
  ICC_EX = 15,  // External flag is set
  UNKNOWN
};

inline const char *toCondCodeString(CondCode CC) {
  switch (CC) {
  case ICC_EU:
    return "";
  case ICC_ZE:
    return "_ze";
  case ICC_NZ:
    return "_nz";
  case ICC_PO:
    return "_po";
  case ICC_NE:
    return "_ne";
  case ICC_AZ:
    return "_az";
  case ICC_ZB:
    return "_zb";
  case ICC_CA:
    return "_ca";
  case ICC_NC:
    return "_nc";
  case ICC_CZ:
    return "_cz";
  case ICC_SPO:
    return "_spo";
  case ICC_SNE:
    return "_sne";
  case ICC_NBS:
    return "_nbs";
  case ICC_NEX:
    return "_nex";
  case ICC_BS:
    return "_bs";
  case ICC_EX:
    return "_ex";
  default:
    llvm_unreachable("Unknown condition code");
  }
}

inline CondCode parseCondCode(StringRef Name) {
  return StringSwitch<CondCode>(Name)
      .Case("ze", ICC_ZE)
      .Case("nz", ICC_NZ)
      .Case("po", ICC_PO)
      .Case("ne", ICC_NE)
      .Case("az", ICC_AZ)
      .Case("zb", ICC_ZB)
      .Case("ca", ICC_CA)
      .Case("nc", ICC_NC)
      .Case("cz", ICC_CZ)
      .Case("spo", ICC_SPO)
      .Case("sne", ICC_SNE)
      .Case("nbs", ICC_NBS)
      .Case("nex", ICC_NEX)
      .Case("bs", ICC_BS)
      .Case("ex", ICC_EX)
      .Default(UNKNOWN);
}

inline CondCode getReversedCondCode(CondCode CC) {
  switch (CC) {
  case ICC_ZE:
    return ICC_NZ;
  case ICC_NZ:
    return ICC_ZE;
  case ICC_PO:
    return ICC_NE;
  case ICC_NE:
    return ICC_PO;
  case ICC_AZ:
    return ICC_ZB;
  case ICC_ZB:
    return ICC_AZ;
  case ICC_CA:
    return ICC_NC;
  case ICC_NC:
    return ICC_CA;
  default:
    return UNKNOWN;
  }
}

} // namespace EZHCC
} // namespace llvm

#endif // LLVM_LIB_TARGET_EZH_EZHCONDCODE_H
