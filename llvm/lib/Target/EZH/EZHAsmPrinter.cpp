//===-- EZHAsmPrinter.cpp - EZH LLVM assembly writer ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a printer that converts from our internal representation
// of machine-dependent LLVM code to the EZH assembly language.
//
//===----------------------------------------------------------------------===//

#include "EZHAluCode.h"
#include "EZHCondCode.h"
#include "EZHMCInstLower.h"
#include "EZHTargetMachine.h"
#include "MCTargetDesc/EZHInstPrinter.h"
#include "TargetInfo/EZHTargetInfo.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/IR/Mangler.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "asm-printer"

using namespace llvm;

namespace {
class EZHAsmPrinter : public AsmPrinter {
public:
  explicit EZHAsmPrinter(TargetMachine &TM,
                           std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer), ID) {}

  StringRef getPassName() const override { return "EZH Assembly Printer"; }

  void printOperand(const MachineInstr *MI, int OpNum, raw_ostream &O);
  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode, raw_ostream &O) override;
  void emitInstruction(const MachineInstr *MI) override;
  bool isBlockOnlyReachableByFallthrough(
      const MachineBasicBlock *MBB) const override;

private:
  void customEmitInstruction(const MachineInstr *MI);
  void emitCallInstruction(const MachineInstr *MI);

public:
  static char ID;
};
} // end of anonymous namespace

void EZHAsmPrinter::printOperand(const MachineInstr *MI, int OpNum,
                                   raw_ostream &O) {
  const MachineOperand &MO = MI->getOperand(OpNum);

  switch (MO.getType()) {
  case MachineOperand::MO_Register:
    O << EZHInstPrinter::getRegisterName(MO.getReg());
    break;

  case MachineOperand::MO_Immediate:
    O << MO.getImm();
    break;

  case MachineOperand::MO_MachineBasicBlock:
    O << *MO.getMBB()->getSymbol();
    break;

  case MachineOperand::MO_GlobalAddress:
    O << *getSymbol(MO.getGlobal());
    break;

  case MachineOperand::MO_BlockAddress: {
    MCSymbol *BA = GetBlockAddressSymbol(MO.getBlockAddress());
    O << BA->getName();
    break;
  }

  case MachineOperand::MO_ExternalSymbol:
    O << *GetExternalSymbolSymbol(MO.getSymbolName());
    break;

  case MachineOperand::MO_JumpTableIndex:
    O << MAI.getInternalSymbolPrefix() << "JTI" << getFunctionNumber() << '_'
      << MO.getIndex();
    break;

  case MachineOperand::MO_ConstantPoolIndex:
    O << MAI.getInternalSymbolPrefix() << "CPI" << getFunctionNumber() << '_'
      << MO.getIndex();
    return;

  default:
    llvm_unreachable("<unknown operand type>");
  }
}

// PrintAsmOperand - Print out an operand for an inline asm expression.
bool EZHAsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                                      const char *ExtraCode, raw_ostream &O) {
  // Does this asm operand have a single letter operand modifier?
  if (ExtraCode && ExtraCode[0]) {
    if (ExtraCode[1])
      return true; // Unknown modifier.

    switch (ExtraCode[0]) {
    // The highest-numbered register of a pair.
    case 'H': {
      if (OpNo == 0)
        return true;
      const MachineOperand &FlagsOP = MI->getOperand(OpNo - 1);
      if (!FlagsOP.isImm())
        return true;
      const InlineAsm::Flag Flags(FlagsOP.getImm());
      const unsigned NumVals = Flags.getNumOperandRegisters();
      if (NumVals != 2)
        return true;
      unsigned RegOp = OpNo + 1;
      if (RegOp >= MI->getNumOperands())
        return true;
      const MachineOperand &MO = MI->getOperand(RegOp);
      if (!MO.isReg())
        return true;
      Register Reg = MO.getReg();
      O << EZHInstPrinter::getRegisterName(Reg);
      return false;
    }
    default:
      return AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, O);
    }
  }
  printOperand(MI, OpNo, O);
  return false;
}

//===----------------------------------------------------------------------===//
void EZHAsmPrinter::emitCallInstruction(const MachineInstr *MI) {
  assert((MI->getOpcode() == EZH::CALL || MI->getOpcode() == EZH::CALLR) &&
         "Unsupported call function");

  EZHMCInstLower MCInstLowering(OutContext, *this);
  MCSubtargetInfo STI = getSubtargetInfo();
  // Insert save rca instruction immediately before the call.
  // TODO: We should generate a pc-relative mov instruction here instead
  // of pc + 16 (should be mov .+16 %rca).
  OutStreamer->emitInstruction(MCInstBuilder(EZH::ADD_I_LO)
                                   .addReg(EZH::RCA)
                                   .addReg(EZH::PC)
                                   .addImm(16),
                               STI);

  // Push rca onto the stack.
  //   st %rca, [--%sp]
  OutStreamer->emitInstruction(MCInstBuilder(EZH::SW_RI)
                                   .addReg(EZH::RCA)
                                   .addReg(EZH::SP)
                                   .addImm(-4)
                                   .addImm(LPAC::makePreOp(LPAC::ADD)),
                               STI);

  // Lower the call instruction.
  if (MI->getOpcode() == EZH::CALL) {
    MCInst TmpInst;
    MCInstLowering.Lower(MI, TmpInst);
    TmpInst.setOpcode(EZH::BT);
    OutStreamer->emitInstruction(TmpInst, STI);
  } else {
    OutStreamer->emitInstruction(MCInstBuilder(EZH::ADD_R)
                                     .addReg(EZH::PC)
                                     .addReg(MI->getOperand(0).getReg())
                                     .addReg(EZH::R0)
                                     .addImm(LPCC::ICC_T),
                                 STI);
  }
}

void EZHAsmPrinter::customEmitInstruction(const MachineInstr *MI) {
  EZHMCInstLower MCInstLowering(OutContext, *this);
  MCSubtargetInfo STI = getSubtargetInfo();
  MCInst TmpInst;
  MCInstLowering.Lower(MI, TmpInst);
  OutStreamer->emitInstruction(TmpInst, STI);
}

void EZHAsmPrinter::emitInstruction(const MachineInstr *MI) {
  EZH_MC::verifyInstructionPredicates(MI->getOpcode(),
                                        getSubtargetInfo().getFeatureBits());

  MachineBasicBlock::const_instr_iterator I = MI->getIterator();
  MachineBasicBlock::const_instr_iterator E = MI->getParent()->instr_end();

  do {
    if (I->isCall()) {
      emitCallInstruction(&*I);
      continue;
    }

    customEmitInstruction(&*I);
  } while ((++I != E) && I->isInsideBundle());
}

// isBlockOnlyReachableByFallthough - Return true if the basic block has
// exactly one predecessor and the control transfer mechanism between
// the predecessor and this block is a fall-through.
// FIXME: could the overridden cases be handled in analyzeBranch?
bool EZHAsmPrinter::isBlockOnlyReachableByFallthrough(
    const MachineBasicBlock *MBB) const {
  // The predecessor has to be immediately before this block.
  const MachineBasicBlock *Pred = *MBB->pred_begin();

  // If the predecessor is a switch statement, assume a jump table
  // implementation, so it is not a fall through.
  if (const BasicBlock *B = Pred->getBasicBlock())
    if (isa<SwitchInst>(B->getTerminator()))
      return false;

  // Check default implementation
  if (!AsmPrinter::isBlockOnlyReachableByFallthrough(MBB))
    return false;

  // Otherwise, check the last instruction.
  // Check if the last terminator is an unconditional branch.
  MachineBasicBlock::const_iterator I = Pred->end();
  while (I != Pred->begin() && !(--I)->isTerminator()) {
  }

  return !I->isBarrier();
}

char EZHAsmPrinter::ID = 0;

INITIALIZE_PASS(EZHAsmPrinter, "ezh-asm-printer", "EZH Assembly Printer",
                false, false)

// Force static initialization.
extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeEZHAsmPrinter() {
  RegisterAsmPrinter<EZHAsmPrinter> X(getTheEZHTarget());
}
