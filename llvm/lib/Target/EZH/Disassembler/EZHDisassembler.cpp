//===- EZHDisassembler.cpp - Disassembler for EZH -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file is part of the EZH Disassembler.
//
//===----------------------------------------------------------------------===//

#include "EZHDisassembler.h"

#include "EZHAluCode.h"
#include "EZHCondCode.h"
#include "EZHInstrInfo.h"
#include "TargetInfo/EZHTargetInfo.h"
#include "llvm/MC/MCDecoder.h"
#include "llvm/MC/MCDecoderOps.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MathExtras.h"

#define DEBUG_TYPE "ezh-disassembler"

using namespace llvm;
using namespace llvm::MCD;

typedef MCDisassembler::DecodeStatus DecodeStatus;

static MCDisassembler *createEZHDisassembler(const Target & /*T*/,
                                             const MCSubtargetInfo &STI,
                                             MCContext &Ctx) {
  return new EZHDisassembler(STI, Ctx);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeEZHDisassembler() {
  // Register the disassembler
  TargetRegistry::RegisterMCDisassembler(getTheEZHTarget(),
                                         createEZHDisassembler);
}

EZHDisassembler::EZHDisassembler(const MCSubtargetInfo &STI, MCContext &Ctx)
    : MCDisassembler(STI, Ctx) {}

// clang-format off
static const unsigned GPRDecoderTable[] = {
  EZH::R0,  EZH::R1,  EZH::PC,  EZH::R3,  EZH::SP,  EZH::FP,
  EZH::R6,  EZH::R7,  EZH::RV,  EZH::R9,  EZH::RR1, EZH::RR2,
  EZH::R12, EZH::R13, EZH::R14, EZH::RCA, EZH::R16, EZH::R17,
  EZH::R18, EZH::R19, EZH::R20, EZH::R21, EZH::R22, EZH::R23,
  EZH::R24, EZH::R25, EZH::R26, EZH::R27, EZH::R28, EZH::R29,
  EZH::R30, EZH::R31
};
// clang-format on

DecodeStatus DecodeGPRRegisterClass(MCInst &Inst, unsigned RegNo,
                                    uint64_t /*Address*/,
                                    const MCDisassembler * /*Decoder*/) {
  if (RegNo > 31)
    return MCDisassembler::Fail;

  unsigned Reg = GPRDecoderTable[RegNo];
  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

static DecodeStatus decodeRiMemoryValue(MCInst &Inst, unsigned Insn,
                                        uint64_t Address,
                                        const MCDisassembler *Decoder) {
  // RI memory values encoded using 23 bits:
  //   5 bit register, 16 bit constant
  unsigned Register = (Insn >> 18) & 0x1f;
  Inst.addOperand(MCOperand::createReg(GPRDecoderTable[Register]));
  unsigned Offset = (Insn & 0xffff);
  Inst.addOperand(MCOperand::createImm(SignExtend32<16>(Offset)));

  return MCDisassembler::Success;
}

static DecodeStatus decodeRrMemoryValue(MCInst &Inst, unsigned Insn,
                                        uint64_t Address,
                                        const MCDisassembler *Decoder) {
  // RR memory values encoded using 20 bits:
  //   5 bit register, 5 bit register, 2 bit PQ, 3 bit ALU operator, 5 bit JJJJJ
  unsigned Register = (Insn >> 15) & 0x1f;
  Inst.addOperand(MCOperand::createReg(GPRDecoderTable[Register]));
  Register = (Insn >> 10) & 0x1f;
  Inst.addOperand(MCOperand::createReg(GPRDecoderTable[Register]));

  return MCDisassembler::Success;
}

static DecodeStatus decodeSplsValue(MCInst &Inst, unsigned Insn,
                                    uint64_t Address,
                                    const MCDisassembler *Decoder) {
  // RI memory values encoded using 17 bits:
  //   5 bit register, 10 bit constant
  unsigned Register = (Insn >> 12) & 0x1f;
  Inst.addOperand(MCOperand::createReg(GPRDecoderTable[Register]));
  unsigned Offset = (Insn & 0x3ff);
  Inst.addOperand(MCOperand::createImm(SignExtend32<10>(Offset)));

  return MCDisassembler::Success;
}

static bool tryAddingSymbolicOperand(int64_t Value, bool IsBranch,
                                     uint64_t Address, uint64_t Offset,
                                     uint64_t Width, MCInst &MI,
                                     const MCDisassembler *Decoder) {
  return Decoder->tryAddingSymbolicOperand(MI, Value, Address, IsBranch, Offset,
                                           Width, /*InstSize=*/0);
}

static DecodeStatus decodeBranch(MCInst &MI, unsigned Insn, uint64_t Address,
                                 const MCDisassembler *Decoder) {
  if (!tryAddingSymbolicOperand(Insn + Address, false, Address, 2, 23, MI,
                                Decoder))
    MI.addOperand(MCOperand::createImm(Insn));
  return MCDisassembler::Success;
}

static DecodeStatus decodeShiftImm(MCInst &Inst, unsigned Insn,
                                   uint64_t Address,
                                   const MCDisassembler *Decoder) {
  unsigned Offset = (Insn & 0xffff);
  Inst.addOperand(MCOperand::createImm(SignExtend32<16>(Offset)));

  return MCDisassembler::Success;
}

static DecodeStatus decodePredicateOperand(MCInst &Inst, unsigned Val,
                                           uint64_t Address,
                                           const MCDisassembler *Decoder) {
  if (Val >= LPCC::UNKNOWN)
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createImm(Val));
  return MCDisassembler::Success;
}

#include "EZHGenDisassemblerTables.inc"

static DecodeStatus readInstruction32(ArrayRef<uint8_t> Bytes, uint64_t &Size,
                                      uint32_t &Insn) {
  // We want to read exactly 4 bytes of data.
  if (Bytes.size() < 4) {
    Size = 0;
    return MCDisassembler::Fail;
  }

  // Encoded as big-endian 32-bit word in the stream.
  Insn =
      (Bytes[0] << 24) | (Bytes[1] << 16) | (Bytes[2] << 8) | (Bytes[3] << 0);

  return MCDisassembler::Success;
}

static void PostOperandDecodeAdjust(MCInst &Instr, uint32_t Insn) {
  unsigned AluOp = LPAC::ADD;
  // Fix up for pre and post operations.
  int PqShift = -1;
  if (isRMOpcode(Instr.getOpcode()))
    PqShift = 16;
  else if (isSPLSOpcode(Instr.getOpcode()))
    PqShift = 10;
  else if (isRRMOpcode(Instr.getOpcode())) {
    PqShift = 16;
    // Determine RRM ALU op.
    AluOp = (Insn >> 8) & 0x7;
    if (AluOp == 7)
      // Handle JJJJJ
      // 0b10000 or 0b11000
      AluOp |= 0x20 | (((Insn >> 3) & 0xf) << 1);
  }

  if (PqShift != -1) {
    unsigned PQ = (Insn >> PqShift) & 0x3;
    switch (PQ) {
    case 0x0:
      if (Instr.getOperand(2).isReg()) {
        Instr.getOperand(2).setReg(EZH::R0);
      }
      if (Instr.getOperand(2).isImm())
        Instr.getOperand(2).setImm(0);
      break;
    case 0x1:
      AluOp = LPAC::makePostOp(AluOp);
      break;
    case 0x2:
      break;
    case 0x3:
      AluOp = LPAC::makePreOp(AluOp);
      break;
    }
    Instr.addOperand(MCOperand::createImm(AluOp));
  }
}

DecodeStatus EZHDisassembler::getInstruction(MCInst &Instr, uint64_t &Size,
                                             ArrayRef<uint8_t> Bytes,
                                             uint64_t Address,
                                             raw_ostream & /*CStream*/) const {
  uint32_t Insn;

  DecodeStatus Result = readInstruction32(Bytes, Size, Insn);

  if (Result == MCDisassembler::Fail)
    return MCDisassembler::Fail;

  // Call auto-generated decoder function
  Result =
      decodeInstruction(DecoderTableEZH32, Instr, Insn, Address, this, STI);

  if (Result != MCDisassembler::Fail) {
    PostOperandDecodeAdjust(Instr, Insn);
    Size = 4;
    return Result;
  }

  return MCDisassembler::Fail;
}
