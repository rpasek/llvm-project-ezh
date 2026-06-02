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
// Description:
//   Implements machine code decoding logic using TableGen-generated decoder
//   tables to reconstruct EZH assembly instructions.
//
// Copied From:
//   Lanai target backend
//   (llvm/lib/Target/Lanai/Disassembler/LanaiDisassembler.cpp).
//
// Changes:
//   Integrated TableGen decoder tables (EZHGenDisassemblerTables.inc); handled
//   custom register decoding and branch immediate sign-extension matching
//   fsl_smartdma_prv.h.
//
//===----------------------------------------------------------------------===//

#include "EZHDisassembler.h"

#include "EZHCondCode.h"
#include "EZHInstrInfo.h"
#include "MCTargetDesc/EZHMCTargetDesc.h"
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
    EZH::R0, EZH::R1, EZH::R2,  EZH::R3,  EZH::R4,  EZH::R5,
    EZH::R6, EZH::R7, EZH::GPO, EZH::GPD, EZH::CFS, EZH::CFM,
    EZH::SP, EZH::PC, EZH::GPI, EZH::RA};
// clang-format on

DecodeStatus DecodeGPRRegisterClass(MCInst &Inst, unsigned RegNo,
                                    uint64_t /*Address*/,
                                    const MCDisassembler * /*Decoder*/) {
  if (RegNo > 15)
    return MCDisassembler::Fail;

  unsigned Reg = GPRDecoderTable[RegNo];
  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

static DecodeStatus DecodeGPR3BitRegisterClass(MCInst &Inst, unsigned RegNo,
                                               uint64_t Address,
                                               const MCDisassembler *Decoder) {
  if (RegNo > 7)
    return MCDisassembler::Fail;

  unsigned Reg = GPRDecoderTable[RegNo];
  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

static DecodeStatus decodeSimm12(MCInst &Inst, uint64_t Imm, uint64_t Address,
                                 const MCDisassembler *Decoder) {
  Inst.addOperand(MCOperand::createImm(SignExtend64<12>(Imm)));
  return MCDisassembler::Success;
}

static DecodeStatus decodeSimm11(MCInst &Inst, uint64_t Imm, uint64_t Address,
                                 const MCDisassembler *Decoder) {
  Inst.addOperand(MCOperand::createImm(SignExtend64<11>(Imm)));
  return MCDisassembler::Success;
}

static DecodeStatus decodeWordOffset(MCInst &Inst, uint64_t Imm,
                                     uint64_t Address,
                                     const MCDisassembler *Decoder) {
  Inst.addOperand(MCOperand::createImm(SignExtend64<8>(Imm) << 2));
  return MCDisassembler::Success;
}

static DecodeStatus decodeSimm8(MCInst &Inst, uint64_t Imm, uint64_t Address,
                                const MCDisassembler *Decoder) {
  Inst.addOperand(MCOperand::createImm(SignExtend64<8>(Imm)));
  return MCDisassembler::Success;
}

static DecodeStatus decodeSimm21(MCInst &Inst, uint64_t Imm, uint64_t Address,
                                 const MCDisassembler *Decoder) {
  // Imm is a 21-bit word address (23-bit byte address).
  // EZH absolute branches replace the lower 23 bits of the current PC.
  uint64_t Target = (Address & ~0x7FFFFFULL) | ((Imm & 0x1FFFFF) * 4);
  Inst.addOperand(MCOperand::createImm(Target));
  return MCDisassembler::Success;
}

static DecodeStatus decodeSimm30(MCInst &Inst, uint64_t Imm, uint64_t Address,
                                 const MCDisassembler *Decoder) {
  uint64_t Target = (Imm & 0x3FFFFFFF) << 2;
  Inst.addOperand(MCOperand::createImm(Target));
  return MCDisassembler::Success;
}

static DecodeStatus decodeSimm16(MCInst &Inst, uint64_t Imm, uint64_t Address,
                                 const MCDisassembler *Decoder) {
  Inst.addOperand(MCOperand::createImm(SignExtend64<16>(Imm)));
  return MCDisassembler::Success;
}

static DecodeStatus decodePredicateOperand(MCInst &Inst, uint64_t Val,
                                           uint64_t Address,
                                           const MCDisassembler *Decoder) {
  if (Val >= EZHCC::UNKNOWN)
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

  // Little endian decoding
  Insn =
      (Bytes[3] << 24) | (Bytes[2] << 16) | (Bytes[1] << 8) | (Bytes[0] << 0);
  Size = 4;
  return MCDisassembler::Success;
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
    return Result;
  }

  return MCDisassembler::Fail;
}
