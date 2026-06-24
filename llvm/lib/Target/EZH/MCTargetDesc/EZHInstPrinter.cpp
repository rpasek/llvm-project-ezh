//===-- EZHInstPrinter.cpp - Convert EZH MCInst to asm syntax ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This class prints an EZH MCInst to a .s file.
//
//===----------------------------------------------------------------------===//

#include "EZHInstPrinter.h"
#include "EZHAluCode.h"
#include "EZHCondCode.h"
#include "MCTargetDesc/EZHMCTargetDesc.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

// Include the auto-generated portion of the assembly writer.
#define PRINT_ALIAS_INSTR
#include "EZHGenAsmWriter.inc"

void EZHInstPrinter::printRegName(raw_ostream &OS, MCRegister Reg) {
  OS << StringRef(getRegisterName(Reg)).lower();
}

bool EZHInstPrinter::printInst(const MCInst *MI, raw_ostream &OS,
                                 StringRef Alias, unsigned OpNo0,
                                 unsigned OpNo1) {
  OS << "\t" << Alias << " ";
  printOperand(MI, OpNo0, OS);
  OS << ", ";
  printOperand(MI, OpNo1, OS);
  return true;
}

static bool usesGivenOffset(const MCInst *MI, int AddOffset) {
  unsigned AluCode = MI->getOperand(3).getImm();
  return LPAC::encodeEZHAluCode(AluCode) == LPAC::ADD &&
         (MI->getOperand(2).getImm() == AddOffset ||
          MI->getOperand(2).getImm() == -AddOffset);
}

static bool isPreIncrementForm(const MCInst *MI, int AddOffset) {
  unsigned AluCode = MI->getOperand(3).getImm();
  return LPAC::isPreOp(AluCode) && usesGivenOffset(MI, AddOffset);
}

static bool isPostIncrementForm(const MCInst *MI, int AddOffset) {
  unsigned AluCode = MI->getOperand(3).getImm();
  return LPAC::isPostOp(AluCode) && usesGivenOffset(MI, AddOffset);
}

static StringRef decIncOperator(const MCInst *MI) {
  if (MI->getOperand(2).getImm() < 0)
    return "--";
  return "++";
}

bool EZHInstPrinter::printMemoryLoadIncrement(const MCInst *MI,
                                                raw_ostream &OS,
                                                StringRef Opcode,
                                                int AddOffset) {
  if (isPreIncrementForm(MI, AddOffset)) {
    OS << "\t" << Opcode << "\t[" << decIncOperator(MI) << "%"
       << getRegisterName(MI->getOperand(1).getReg()) << "], %"
       << getRegisterName(MI->getOperand(0).getReg());
    return true;
  }
  if (isPostIncrementForm(MI, AddOffset)) {
    OS << "\t" << Opcode << "\t[%"
       << getRegisterName(MI->getOperand(1).getReg()) << decIncOperator(MI)
       << "], %" << getRegisterName(MI->getOperand(0).getReg());
    return true;
  }
  return false;
}

bool EZHInstPrinter::printMemoryStoreIncrement(const MCInst *MI,
                                                 raw_ostream &OS,
                                                 StringRef Opcode,
                                                 int AddOffset) {
  if (isPreIncrementForm(MI, AddOffset)) {
    OS << "\t" << Opcode << "\t%" << getRegisterName(MI->getOperand(0).getReg())
       << ", [" << decIncOperator(MI) << "%"
       << getRegisterName(MI->getOperand(1).getReg()) << "]";
    return true;
  }
  if (isPostIncrementForm(MI, AddOffset)) {
    OS << "\t" << Opcode << "\t%" << getRegisterName(MI->getOperand(0).getReg())
       << ", [%" << getRegisterName(MI->getOperand(1).getReg())
       << decIncOperator(MI) << "]";
    return true;
  }
  return false;
}

bool EZHInstPrinter::printAlias(const MCInst *MI, raw_ostream &OS) {
  switch (MI->getOpcode()) {
  case EZH::LDW_RI:
    // ld 4[*%rN], %rX => ld [++imm], %rX
    // ld -4[*%rN], %rX => ld [--imm], %rX
    // ld 4[%rN*], %rX => ld [imm++], %rX
    // ld -4[%rN*], %rX => ld [imm--], %rX
    return printMemoryLoadIncrement(MI, OS, "ld", 4);
  case EZH::LDHs_RI:
    return printMemoryLoadIncrement(MI, OS, "ld.h", 2);
  case EZH::LDHz_RI:
    return printMemoryLoadIncrement(MI, OS, "uld.h", 2);
  case EZH::LDBs_RI:
    return printMemoryLoadIncrement(MI, OS, "ld.b", 1);
  case EZH::LDBz_RI:
    return printMemoryLoadIncrement(MI, OS, "uld.b", 1);
  case EZH::SW_RI:
    // st %rX, 4[*%rN] => st %rX, [++imm]
    // st %rX, -4[*%rN] => st %rX, [--imm]
    // st %rX, 4[%rN*] => st %rX, [imm++]
    // st %rX, -4[%rN*] => st %rX, [imm--]
    return printMemoryStoreIncrement(MI, OS, "st", 4);
  case EZH::STH_RI:
    return printMemoryStoreIncrement(MI, OS, "st.h", 2);
  case EZH::STB_RI:
    return printMemoryStoreIncrement(MI, OS, "st.b", 1);
  default:
    return false;
  }
}

void EZHInstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                 StringRef Annotation,
                                 const MCSubtargetInfo & /*STI*/,
                                 raw_ostream &OS) {
  if (!printAlias(MI, OS) && !printAliasInstr(MI, Address, OS))
    printInstruction(MI, Address, OS);
  printAnnotation(OS, Annotation);
}

void EZHInstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                    raw_ostream &OS) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg())
    OS << "%" << getRegisterName(Op.getReg());
  else if (Op.isImm())
    OS << formatHex(Op.getImm());
  else {
    assert(Op.isExpr() && "Expected an expression");
    MAI.printExpr(OS, *Op.getExpr());
  }
}

void EZHInstPrinter::printMemImmOperand(const MCInst *MI, unsigned OpNo,
                                          raw_ostream &OS) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isImm()) {
    OS << '[' << formatHex(Op.getImm()) << ']';
  } else {
    // Symbolic operand will be lowered to immediate value by linker
    assert(Op.isExpr() && "Expected an expression");
    OS << '[';
    MAI.printExpr(OS, *Op.getExpr());
    OS << ']';
  }
}

void EZHInstPrinter::printHi16ImmOperand(const MCInst *MI, unsigned OpNo,
                                           raw_ostream &OS) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isImm()) {
    OS << formatHex(Op.getImm() << 16);
  } else {
    // Symbolic operand will be lowered to immediate value by linker
    assert(Op.isExpr() && "Expected an expression");
    MAI.printExpr(OS, *Op.getExpr());
  }
}

void EZHInstPrinter::printHi16AndImmOperand(const MCInst *MI, unsigned OpNo,
                                              raw_ostream &OS) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isImm()) {
    OS << formatHex((Op.getImm() << 16) | 0xffff);
  } else {
    // Symbolic operand will be lowered to immediate value by linker
    assert(Op.isExpr() && "Expected an expression");
    MAI.printExpr(OS, *Op.getExpr());
  }
}

void EZHInstPrinter::printLo16AndImmOperand(const MCInst *MI, unsigned OpNo,
                                              raw_ostream &OS) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isImm()) {
    OS << formatHex(0xffff0000 | Op.getImm());
  } else {
    // Symbolic operand will be lowered to immediate value by linker
    assert(Op.isExpr() && "Expected an expression");
    MAI.printExpr(OS, *Op.getExpr());
  }
}

static void printMemoryBaseRegister(raw_ostream &OS, const unsigned AluCode,
                                    const MCOperand &RegOp) {
  assert(RegOp.isReg() && "Register operand expected");
  OS << "[";
  if (LPAC::isPreOp(AluCode))
    OS << "*";
  OS << "%" << EZHInstPrinter::getRegisterName(RegOp.getReg());
  if (LPAC::isPostOp(AluCode))
    OS << "*";
  OS << "]";
}

template <unsigned SizeInBits>
static void printMemoryImmediateOffset(const MCAsmInfo &MAI,
                                       const MCOperand &OffsetOp,
                                       raw_ostream &OS) {
  assert((OffsetOp.isImm() || OffsetOp.isExpr()) && "Immediate expected");
  if (OffsetOp.isImm()) {
    assert(isInt<SizeInBits>(OffsetOp.getImm()) && "Constant value truncated");
    OS << OffsetOp.getImm();
  } else
    MAI.printExpr(OS, *OffsetOp.getExpr());
}

void EZHInstPrinter::printMemRiOperand(const MCInst *MI, int OpNo,
                                         raw_ostream &OS) {
  const MCOperand &RegOp = MI->getOperand(OpNo);
  const MCOperand &OffsetOp = MI->getOperand(OpNo + 1);
  const MCOperand &AluOp = MI->getOperand(OpNo + 2);
  const unsigned AluCode = AluOp.getImm();

  // Offset
  printMemoryImmediateOffset<16>(MAI, OffsetOp, OS);

  // Register
  printMemoryBaseRegister(OS, AluCode, RegOp);
}

void EZHInstPrinter::printMemRrOperand(const MCInst *MI, int OpNo,
                                         raw_ostream &OS) {
  const MCOperand &RegOp = MI->getOperand(OpNo);
  const MCOperand &OffsetOp = MI->getOperand(OpNo + 1);
  const MCOperand &AluOp = MI->getOperand(OpNo + 2);
  const unsigned AluCode = AluOp.getImm();
  assert(OffsetOp.isReg() && RegOp.isReg() && "Registers expected.");

  // [ Base OP Offset ]
  OS << "[";
  if (LPAC::isPreOp(AluCode))
    OS << "*";
  OS << "%" << getRegisterName(RegOp.getReg());
  if (LPAC::isPostOp(AluCode))
    OS << "*";
  OS << " " << LPAC::ezhAluCodeToString(AluCode) << " ";
  OS << "%" << getRegisterName(OffsetOp.getReg());
  OS << "]";
}

void EZHInstPrinter::printMemSplsOperand(const MCInst *MI, int OpNo,
                                           raw_ostream &OS) {
  const MCOperand &RegOp = MI->getOperand(OpNo);
  const MCOperand &OffsetOp = MI->getOperand(OpNo + 1);
  const MCOperand &AluOp = MI->getOperand(OpNo + 2);
  const unsigned AluCode = AluOp.getImm();

  // Offset
  printMemoryImmediateOffset<10>(MAI, OffsetOp, OS);

  // Register
  printMemoryBaseRegister(OS, AluCode, RegOp);
}

void EZHInstPrinter::printCCOperand(const MCInst *MI, int OpNo,
                                      raw_ostream &OS) {
  LPCC::CondCode CC =
      static_cast<LPCC::CondCode>(MI->getOperand(OpNo).getImm());
  // Handle the undefined value here for printing so we don't abort().
  if (CC >= LPCC::UNKNOWN)
    OS << "<und>";
  else
    OS << ezhCondCodeToString(CC);
}

void EZHInstPrinter::printPredicateOperand(const MCInst *MI, unsigned OpNo,
                                             raw_ostream &OS) {
  LPCC::CondCode CC =
      static_cast<LPCC::CondCode>(MI->getOperand(OpNo).getImm());
  // Handle the undefined value here for printing so we don't abort().
  if (CC >= LPCC::UNKNOWN)
    OS << "<und>";
  else if (CC != LPCC::ICC_T)
    OS << "." << ezhCondCodeToString(CC);
}
