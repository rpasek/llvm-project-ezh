//===-- EZHMCAsmInfo.h - EZH asm properties ------------------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the EZHMCAsmInfo class.
//
// Description:
//   Declares EZHMCAsmInfo, specifying assembly syntax conventions, comment
//   characters, and data directive formatting.
//
// Copied From:
//   Lanai target backend
//   (llvm/lib/Target/Lanai/MCTargetDesc/LanaiMCAsmInfo.h).
//
// Changes:
//   Renamed LanaiMCAsmInfo to EZHMCAsmInfo.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EZH_MCTARGETDESC_EZHMCASMINFO_H
#define LLVM_LIB_TARGET_EZH_MCTARGETDESC_EZHMCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {
class Triple;

class EZHMCAsmInfo : public MCAsmInfoELF {
  void anchor() override;

public:
  explicit EZHMCAsmInfo(const Triple &TheTriple,
                        const MCTargetOptions &Options);
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_EZH_MCTARGETDESC_EZHMCASMINFO_H
