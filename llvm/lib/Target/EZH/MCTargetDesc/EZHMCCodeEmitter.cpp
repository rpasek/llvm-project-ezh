//===-- EZHMCCodeEmitter.cpp - Convert EZH code to machine code -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the EZHMCCodeEmitter class.
//
// Description:
//   Implements EZHMCCodeEmitter, lowering MCInst structures into raw binary
//   machine code stream bytes using TableGen encoding tables.
//
// Copied From:
//   Lanai target backend
//   (llvm/lib/Target/Lanai/MCTargetDesc/LanaiMCCodeEmitter.cpp).
//
// Changes:
//   Integrated TableGen instruction encoding logic (getBinaryCodeForInstr);
//   handled custom operand bit-packing for EZH registers and immediates.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/EZHBaseInfo.h"
#include "MCTargetDesc/EZHFixupKinds.h"
#include "MCTargetDesc/EZHMCTargetDesc.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "mccodeemitter"

namespace {
class EZHMCCodeEmitter : public MCCodeEmitter {
  MCContext &Ctx;

public:
  EZHMCCodeEmitter(const MCInstrInfo & /*mcii*/, MCContext &ctx) : Ctx(ctx) {}

  ~EZHMCCodeEmitter() override = default;

  // getBinaryCodeForInstr - TableGen'erated function.
  uint64_t getBinaryCodeForInstr(const MCInst &MI,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &STI) const;

  // getMachineOpValue - Return binary encoding of operand.
  unsigned getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;

  void encodeInstruction(const MCInst &MI, SmallVectorImpl<char> &CB,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;

  unsigned getBranchTargetOpValue(const MCInst &MI, unsigned OpNo,
                                  SmallVectorImpl<MCFixup> &Fixups,
                                  const MCSubtargetInfo &STI) const;

  unsigned getCallTargetOpValue(const MCInst &MI, unsigned OpNo,
                                SmallVectorImpl<MCFixup> &Fixups,
                                const MCSubtargetInfo &STI) const;

  unsigned getPerAddrOpValue(const MCInst &MI, unsigned OpNo,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;

  unsigned getWordOffsetOpValue(const MCInst &MI, unsigned OpNo,
                                SmallVectorImpl<MCFixup> &Fixups,
                                const MCSubtargetInfo &STI) const;
};

} // end anonymous namespace

unsigned EZHMCCodeEmitter::getMachineOpValue(const MCInst &MI,
                                             const MCOperand &MO,
                                             SmallVectorImpl<MCFixup> &Fixups,
                                             const MCSubtargetInfo &STI) const {
  if (MO.isReg())
    return Ctx.getRegisterInfo()->getEncodingValue(MO.getReg());
  if (MO.isImm()) {
    int64_t Imm = MO.getImm();
    unsigned Opc = MI.getOpcode();

    if (Opc == EZH::LOAD_IMM) {
      if (!isInt<11>(Imm)) {
        Ctx.reportError(MI.getLoc(),
                        "immediate operand " + Twine(Imm) +
                            " is out of range for e_load_imm (requires 11-bit "
                            "signed immediate, -1024 to 1023)!");
        return 0;
      }
    } else if (Opc == EZH::ADD_IMM || Opc == EZH::SUB_IMM) {
      if (!isInt<12>(Imm)) {
        Ctx.reportError(MI.getLoc(),
                        "immediate operand " + Twine(Imm) +
                            " is out of range for e_add/sub_imm (requires "
                            "12-bit signed immediate, -2048 to 2047)!");
        return 0;
      }
    } else if (Opc == EZH::LSL || Opc == EZH::LSR || Opc == EZH::ASR ||
               Opc == EZH::ROR) {
      if (!isUInt<5>(Imm)) {
        Ctx.reportError(MI.getLoc(), "shift count immediate " + Twine(Imm) +
                                         " is out of range (requires 5-bit "
                                         "unsigned immediate, 0 to 31)!");
        return 0;
      }
    }

    return static_cast<unsigned>(Imm);
  }

  if (MO.isExpr()) {
    unsigned Opc = MI.getOpcode();
    unsigned FixupKind = EZH::FIXUP_EZH_32;
    if (Opc == EZH::LOAD_SIMM)
      FixupKind = EZH::FIXUP_EZH_11;
    else if (Opc == EZH::ADD_IMM || Opc == EZH::SUB_IMM ||
             Opc == EZH::ADC_IMM || Opc == EZH::SBC_IMM || Opc == EZH::OR_IMM ||
             Opc == EZH::AND_IMM || Opc == EZH::XOR_IMM)
      FixupKind = EZH::FIXUP_EZH_12;

    Fixups.push_back(MCFixup::create(0, MO.getExpr(), MCFixupKind(FixupKind)));
    return 0;
  }

  return 0;
}

void EZHMCCodeEmitter::encodeInstruction(const MCInst &MI,
                                         SmallVectorImpl<char> &CB,
                                         SmallVectorImpl<MCFixup> &Fixups,
                                         const MCSubtargetInfo &STI) const {
  uint64_t Bits = getBinaryCodeForInstr(MI, Fixups, STI);
  support::endian::write<uint32_t>(CB, Bits, llvm::endianness::little);
}

unsigned
EZHMCCodeEmitter::getBranchTargetOpValue(const MCInst &MI, unsigned OpNo,
                                         SmallVectorImpl<MCFixup> &Fixups,
                                         const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isReg())
    return Ctx.getRegisterInfo()->getEncodingValue(MO.getReg());
  if (MO.isImm()) {
    uint32_t val = static_cast<uint32_t>(MO.getImm());
    assert((val & 3) == 0 && "Branch target not 4-byte aligned!");
    return val >> 2;
  }

  Fixups.push_back(
      MCFixup::create(0, MO.getExpr(), MCFixupKind(EZH::FIXUP_EZH_21), false));
  return 0;
}

unsigned
EZHMCCodeEmitter::getCallTargetOpValue(const MCInst &MI, unsigned OpNo,
                                       SmallVectorImpl<MCFixup> &Fixups,
                                       const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isReg())
    return Ctx.getRegisterInfo()->getEncodingValue(MO.getReg());
  if (MO.isImm()) {
    uint32_t val = static_cast<uint32_t>(MO.getImm());
    assert((val & 3) == 0 && "Call offset not 4-byte aligned!");
    return val >> 2;
  }

  Fixups.push_back(
      MCFixup::create(0, MO.getExpr(), MCFixupKind(EZH::FIXUP_EZH_30)));
  return 0;
}

unsigned EZHMCCodeEmitter::getPerAddrOpValue(const MCInst &MI, unsigned OpNo,
                                             SmallVectorImpl<MCFixup> &Fixups,
                                             const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isImm()) {
    uint64_t Imm = MO.getImm();

    // If it is a full physical address (from assembler), convert to offset
    if (Imm >= 0x40000000 && Imm <= 0x400FFFFF) {
      Imm -= 0x40000000;
    }

    // Now Imm MUST be a valid 20-bit offset
    // Must be 4-byte aligned
    if ((Imm & 3) != 0) {
      std::string Msg;
      raw_string_ostream OS(Msg);
      OS << "peripheral offset " << format_hex(Imm, 10)
         << " is not 4-byte aligned!";
      Ctx.reportError(MI.getLoc(), Msg);
      return 0;
    }
    // Must be in safe offset range: [0, 0xFFFFF]
    if (Imm > 0xFFFFF) {
      std::string Msg;
      raw_string_ostream OS(Msg);
      OS << "peripheral offset " << format_hex(Imm, 10)
         << " is out of range! EZH peripheral instructions "
         << "can only target a 1 MB range!";
      Ctx.reportError(MI.getLoc(), Msg);
      return 0;
    }
    return static_cast<unsigned>(Imm);
  }
  return getMachineOpValue(MI, MO, Fixups, STI);
}

unsigned
EZHMCCodeEmitter::getWordOffsetOpValue(const MCInst &MI, unsigned OpNo,
                                       SmallVectorImpl<MCFixup> &Fixups,
                                       const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isImm()) {
    uint32_t val = static_cast<uint32_t>(MO.getImm());
    assert((val & 3) == 0 && "Word offset not 4-byte aligned!");
    return val >> 2;
  }

  if (MO.isExpr()) {
    Fixups.push_back(MCFixup::create(
        0, MO.getExpr(), MCFixupKind(EZH::FIXUP_EZH_8_PCREL), true));
    return 0;
  }

  return getMachineOpValue(MI, MO, Fixups, STI);
}

MCCodeEmitter *llvm::createEZHMCCodeEmitter(const MCInstrInfo &MCII,
                                            MCContext &Ctx) {
  return new EZHMCCodeEmitter(MCII, Ctx);
}

#include "EZHGenMCCodeEmitter.inc"
