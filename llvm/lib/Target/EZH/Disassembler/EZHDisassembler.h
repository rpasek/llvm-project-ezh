//===- EZHDisassembler.h - Disassembler for EZH -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file is part of the EZH Disassembler.
//
// Description:
//   Declares EZHDisassembler, decoding binary machine code bytes into EZH
//   MCInst representations.
//
// Copied From:
//   Lanai target backend
//   (llvm/lib/Target/Lanai/Disassembler/LanaiDisassembler.h).
//
// Changes:
//   Renamed LanaiDisassembler to EZHDisassembler; declared EZH-specific
//   decoding methods.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EZH_DISASSEMBLER_EZHDISASSEMBLER_H
#define LLVM_LIB_TARGET_EZH_DISASSEMBLER_EZHDISASSEMBLER_H

#include "llvm/MC/MCDisassembler/MCDisassembler.h"

namespace llvm {

class EZHDisassembler : public MCDisassembler {
public:
  EZHDisassembler(const MCSubtargetInfo &STI, MCContext &Ctx);

  ~EZHDisassembler() override = default;

  // getInstruction - See MCDisassembler.
  MCDisassembler::DecodeStatus
  getInstruction(MCInst &Instr, uint64_t &Size, ArrayRef<uint8_t> Bytes,
                 uint64_t Address, raw_ostream &CStream) const override;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EZH_DISASSEMBLER_EZHDISASSEMBLER_H
