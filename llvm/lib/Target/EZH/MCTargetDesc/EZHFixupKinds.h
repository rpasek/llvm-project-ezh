//===-- EZHFixupKinds.h - EZH Specific Fixup Entries --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Description:
//   Enumerates EZH-specific relocation fixup kinds (e.g., PC-relative branch
//   offsets, constant pool relocations).
//
// Copied From:
//   Lanai target backend
//   (llvm/lib/Target/Lanai/MCTargetDesc/LanaiFixupKinds.h).
//
// Changes:
//   Defined fixup enumerations matching EZH immediate branch and load offset
//   bit-widths.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EZH_MCTARGETDESC_EZHFIXUPKINDS_H
#define LLVM_LIB_TARGET_EZH_MCTARGETDESC_EZHFIXUPKINDS_H

#include "llvm/MC/MCFixup.h"

namespace llvm {
namespace EZH {
// Although most of the current fixup types reflect a unique relocation
// one can have multiple fixup types for a given relocation and thus need
// to be uniquely named.
//
// This table *must* be in the save order of
// MCFixupKindInfo Infos[EZH::NumTargetFixupKinds]
// in EZHAsmBackend.cpp.
//
enum Fixups {
  // Results in R_EZH_NONE
  FIXUP_EZH_NONE = FirstTargetFixupKind,

  FIXUP_EZH_11,      // 11-bit immediate for load_simm instructions
  FIXUP_EZH_12,      // 12-bit immediate for ALU instructions
  FIXUP_EZH_21,      // 21-bit symbol relocation for goto/call
  FIXUP_EZH_30,      // 30-bit word relocation for gosub
  FIXUP_EZH_32,      // General 32-bit relocation
  FIXUP_EZH_8_PCREL, // 8-bit PC-relative word offset

  // Marker
  LastTargetFixupKind,
  NumTargetFixupKinds = LastTargetFixupKind - FirstTargetFixupKind
};
} // namespace EZH
} // namespace llvm

#endif // LLVM_LIB_TARGET_EZH_MCTARGETDESC_EZHFIXUPKINDS_H
