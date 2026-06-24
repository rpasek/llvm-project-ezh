//===- EZHConstantPoolValue.cpp - EZH constantpool value ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the EZH specific constantpool value class.
//
//===----------------------------------------------------------------------===//

#include "EZHConstantPoolValue.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

//===----------------------------------------------------------------------===//
// EZHConstantPoolValue
//===----------------------------------------------------------------------===//

EZHConstantPoolValue::EZHConstantPoolValue(Type *Ty, unsigned id,
                                           EZHCP::EZHCPKind kind,
                                           unsigned char PCAdj,
                                           EZHCP::EZHCPModifier modifier,
                                           bool addCurrentAddress)
    : MachineConstantPoolValue(Ty), LabelId(id), Kind(kind), PCAdjust(PCAdj),
      Modifier(modifier), AddCurrentAddress(addCurrentAddress) {}

EZHConstantPoolValue::EZHConstantPoolValue(LLVMContext &C, unsigned id,
                                           EZHCP::EZHCPKind kind,
                                           unsigned char PCAdj,
                                           EZHCP::EZHCPModifier modifier,
                                           bool addCurrentAddress)
    : MachineConstantPoolValue((Type *)Type::getInt32Ty(C)), LabelId(id),
      Kind(kind), PCAdjust(PCAdj), Modifier(modifier),
      AddCurrentAddress(addCurrentAddress) {}

EZHConstantPoolValue::~EZHConstantPoolValue() = default;

StringRef EZHConstantPoolValue::getModifierText() const {
  switch (Modifier) {
    // FIXME: Are these case sensitive? It'd be nice to lower-case all the
    // strings if that's legal.
  case EZHCP::no_modifier:
    return "none";
  case EZHCP::TLSGD:
    return "tlsgd";
  case EZHCP::GOT_PREL:
    return "GOT_PREL";
  case EZHCP::GOTTPOFF:
    return "gottpoff";
  case EZHCP::TPOFF:
    return "tpoff";
  case EZHCP::SBREL:
    return "SBREL";
  case EZHCP::SECREL:
    return "secrel32";
  }
  llvm_unreachable("Unknown modifier!");
}

int EZHConstantPoolValue::getExistingMachineCPValue(MachineConstantPool *CP,
                                                    Align Alignment) {
  llvm_unreachable("Shouldn't be calling this directly!");
}

void EZHConstantPoolValue::addSelectionDAGCSEId(FoldingSetNodeID &ID) {
  ID.AddInteger(LabelId);
  ID.AddInteger(PCAdjust);
}

bool EZHConstantPoolValue::hasSameValue(EZHConstantPoolValue *ACPV) {
  if (ACPV->Kind == Kind && ACPV->PCAdjust == PCAdjust &&
      ACPV->Modifier == Modifier && ACPV->LabelId == LabelId &&
      ACPV->AddCurrentAddress == AddCurrentAddress) {
    // Two PC relative constpool entries containing the same GV address or
    // external symbols. FIXME: What about blockaddress?
    if (Kind == EZHCP::CPValue || Kind == EZHCP::CPExtSymbol)
      return true;
  }
  return false;
}

#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
LLVM_DUMP_METHOD void EZHConstantPoolValue::dump() const {
  errs() << "  " << *this;
}
#endif

void EZHConstantPoolValue::print(raw_ostream &O) const {
  if (Modifier)
    O << "(" << getModifierText() << ")";
  if (PCAdjust != 0) {
    O << "-(LPC" << LabelId << "+" << (unsigned)PCAdjust;
    if (AddCurrentAddress)
      O << "-.";
    O << ")";
  }
}

//===----------------------------------------------------------------------===//
// EZHConstantPoolConstant
//===----------------------------------------------------------------------===//

EZHConstantPoolConstant::EZHConstantPoolConstant(
    Type *Ty, const Constant *C, unsigned ID, EZHCP::EZHCPKind Kind,
    unsigned char PCAdj, EZHCP::EZHCPModifier Modifier, bool AddCurrentAddress)
    : EZHConstantPoolValue(Ty, ID, Kind, PCAdj, Modifier, AddCurrentAddress),
      CVal(C) {}

EZHConstantPoolConstant::EZHConstantPoolConstant(const Constant *C, unsigned ID,
                                                 EZHCP::EZHCPKind Kind,
                                                 unsigned char PCAdj,
                                                 EZHCP::EZHCPModifier Modifier,
                                                 bool AddCurrentAddress)
    : EZHConstantPoolValue((Type *)C->getType(), ID, Kind, PCAdj, Modifier,
                           AddCurrentAddress),
      CVal(C) {}

EZHConstantPoolConstant::EZHConstantPoolConstant(const GlobalVariable *GV,
                                                 const Constant *C)
    : EZHConstantPoolValue((Type *)C->getType(), 0, EZHCP::CPPromotedGlobal, 0,
                           EZHCP::no_modifier, false),
      CVal(C) {
  GVars.insert(GV);
}

EZHConstantPoolConstant *EZHConstantPoolConstant::Create(const Constant *C,
                                                         unsigned ID) {
  return new EZHConstantPoolConstant(C, ID, EZHCP::CPValue, 0,
                                     EZHCP::no_modifier, false);
}

EZHConstantPoolConstant *
EZHConstantPoolConstant::Create(const GlobalVariable *GVar,
                                const Constant *Initializer) {
  return new EZHConstantPoolConstant(GVar, Initializer);
}

EZHConstantPoolConstant *
EZHConstantPoolConstant::Create(const GlobalValue *GV,
                                EZHCP::EZHCPModifier Modifier) {
  return new EZHConstantPoolConstant((Type *)Type::getInt32Ty(GV->getContext()),
                                     GV, 0, EZHCP::CPValue, 0, Modifier, false);
}

EZHConstantPoolConstant *EZHConstantPoolConstant::Create(const Constant *C,
                                                         unsigned ID,
                                                         EZHCP::EZHCPKind Kind,
                                                         unsigned char PCAdj) {
  return new EZHConstantPoolConstant(C, ID, Kind, PCAdj, EZHCP::no_modifier,
                                     false);
}

EZHConstantPoolConstant *EZHConstantPoolConstant::Create(
    const Constant *C, unsigned ID, EZHCP::EZHCPKind Kind, unsigned char PCAdj,
    EZHCP::EZHCPModifier Modifier, bool AddCurrentAddress) {
  return new EZHConstantPoolConstant(C, ID, Kind, PCAdj, Modifier,
                                     AddCurrentAddress);
}

const GlobalValue *EZHConstantPoolConstant::getGV() const {
  return dyn_cast_or_null<GlobalValue>(CVal);
}

const BlockAddress *EZHConstantPoolConstant::getBlockAddress() const {
  return dyn_cast_or_null<BlockAddress>(CVal);
}

int EZHConstantPoolConstant::getExistingMachineCPValue(MachineConstantPool *CP,
                                                       Align Alignment) {
  int index =
      getExistingMachineCPValueImpl<EZHConstantPoolConstant>(CP, Alignment);
  if (index != -1) {
    auto *CPV = static_cast<EZHConstantPoolValue *>(
        CP->getConstants()[index].Val.MachineCPVal);
    auto *Constant = cast<EZHConstantPoolConstant>(CPV);
    Constant->GVars.insert_range(GVars);
  }
  return index;
}

bool EZHConstantPoolConstant::hasSameValue(EZHConstantPoolValue *ACPV) {
  const EZHConstantPoolConstant *ACPC = dyn_cast<EZHConstantPoolConstant>(ACPV);
  return ACPC && ACPC->CVal == CVal && EZHConstantPoolValue::hasSameValue(ACPV);
}

void EZHConstantPoolConstant::addSelectionDAGCSEId(FoldingSetNodeID &ID) {
  ID.AddPointer(CVal);
  for (const auto *GV : GVars)
    ID.AddPointer(GV);
  EZHConstantPoolValue::addSelectionDAGCSEId(ID);
}

void EZHConstantPoolConstant::print(raw_ostream &O) const {
  O << CVal->getName();
  EZHConstantPoolValue::print(O);
}

//===----------------------------------------------------------------------===//
// EZHConstantPoolSymbol
//===----------------------------------------------------------------------===//

EZHConstantPoolSymbol::EZHConstantPoolSymbol(LLVMContext &C, StringRef s,
                                             unsigned id, unsigned char PCAdj,
                                             EZHCP::EZHCPModifier Modifier,
                                             bool AddCurrentAddress)
    : EZHConstantPoolValue(C, id, EZHCP::CPExtSymbol, PCAdj, Modifier,
                           AddCurrentAddress),
      S(std::string(s)) {}

EZHConstantPoolSymbol *EZHConstantPoolSymbol::Create(LLVMContext &C,
                                                     StringRef s, unsigned ID,
                                                     unsigned char PCAdj) {
  return new EZHConstantPoolSymbol(C, s, ID, PCAdj, EZHCP::no_modifier, false);
}

int EZHConstantPoolSymbol::getExistingMachineCPValue(MachineConstantPool *CP,
                                                     Align Alignment) {
  return getExistingMachineCPValueImpl<EZHConstantPoolSymbol>(CP, Alignment);
}

bool EZHConstantPoolSymbol::hasSameValue(EZHConstantPoolValue *ACPV) {
  const EZHConstantPoolSymbol *ACPS = dyn_cast<EZHConstantPoolSymbol>(ACPV);
  return ACPS && ACPS->S == S && EZHConstantPoolValue::hasSameValue(ACPV);
}

void EZHConstantPoolSymbol::addSelectionDAGCSEId(FoldingSetNodeID &ID) {
  ID.AddString(S);
  EZHConstantPoolValue::addSelectionDAGCSEId(ID);
}

void EZHConstantPoolSymbol::print(raw_ostream &O) const {
  O << S;
  EZHConstantPoolValue::print(O);
}

//===----------------------------------------------------------------------===//
// EZHConstantPoolMBB
//===----------------------------------------------------------------------===//

EZHConstantPoolMBB::EZHConstantPoolMBB(LLVMContext &C,
                                       const MachineBasicBlock *mbb,
                                       unsigned id, unsigned char PCAdj,
                                       EZHCP::EZHCPModifier Modifier,
                                       bool AddCurrentAddress)
    : EZHConstantPoolValue(C, id, EZHCP::CPMachineBasicBlock, PCAdj, Modifier,
                           AddCurrentAddress),
      MBB(mbb) {}

EZHConstantPoolMBB *EZHConstantPoolMBB::Create(LLVMContext &C,
                                               const MachineBasicBlock *mbb,
                                               unsigned ID,
                                               unsigned char PCAdj) {
  return new EZHConstantPoolMBB(C, mbb, ID, PCAdj, EZHCP::no_modifier, false);
}

int EZHConstantPoolMBB::getExistingMachineCPValue(MachineConstantPool *CP,
                                                  Align Alignment) {
  return getExistingMachineCPValueImpl<EZHConstantPoolMBB>(CP, Alignment);
}

bool EZHConstantPoolMBB::hasSameValue(EZHConstantPoolValue *ACPV) {
  const EZHConstantPoolMBB *ACPMBB = dyn_cast<EZHConstantPoolMBB>(ACPV);
  return ACPMBB && ACPMBB->MBB == MBB &&
         EZHConstantPoolValue::hasSameValue(ACPV);
}

void EZHConstantPoolMBB::addSelectionDAGCSEId(FoldingSetNodeID &ID) {
  ID.AddPointer(MBB);
  EZHConstantPoolValue::addSelectionDAGCSEId(ID);
}

void EZHConstantPoolMBB::print(raw_ostream &O) const {
  O << printMBBReference(*MBB);
  EZHConstantPoolValue::print(O);
}
