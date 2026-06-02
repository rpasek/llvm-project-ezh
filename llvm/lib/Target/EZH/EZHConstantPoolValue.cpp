//===-- EZHConstantPoolValue.cpp - EZH Constant Pool Value ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the EZH specific constant pool value classes.
//
// Description:
//   Implements EZH-specific constant pool value classes, representing
//   custom PC-relative displacement entries (GlobalValues, ExternalSymbols,
//   BlockAddresses, MachineBasicBlocks) for the constant island pass.
//
// Copied From:
//   ARM target backend (llvm/lib/Target/ARM/ARMConstantPoolValue.cpp).
//
// Changes:
//   Adapted references to EZH (EZHCP namespaces, EZHConstantPoolValue classes);
//   simplified to only support active EZH constant pool kinds.
//
//===----------------------------------------------------------------------===//

#include "EZHConstantPoolValue.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

EZHConstantPoolValue::EZHConstantPoolValue(Type *Ty, unsigned id,
                                           EZHCP::EZHCPKind Kind)
    : MachineConstantPoolValue(Ty), LabelId(id), Kind(Kind) {}

void EZHConstantPoolValue::addSelectionDAGCSEId(FoldingSetNodeID &ID) {
  ID.AddInteger(LabelId);
  ID.AddInteger(Kind);
}

bool EZHConstantPoolValue::hasSameValue(EZHConstantPoolValue *ACPV) {
  return equals(ACPV);
}

//===----------------------------------------------------------------------===//
// EZHConstantPoolConstant
//===----------------------------------------------------------------------===//

EZHConstantPoolConstant::EZHConstantPoolConstant(Type *Ty, const Constant *C,
                                                 int64_t Offset, unsigned ID,
                                                 EZHCP::EZHCPKind Kind)
    : EZHConstantPoolValue(Ty, ID, Kind), CVal(C), Offset(Offset) {}

EZHConstantPoolConstant *EZHConstantPoolConstant::Create(const Constant *C,
                                                         unsigned ID) {
  return new EZHConstantPoolConstant(const_cast<Type *>(C->getType()), C, 0, ID,
                                     EZHCP::CPConstant);
}

EZHConstantPoolConstant *EZHConstantPoolConstant::Create(const GlobalValue *GV,
                                                         int64_t Offset,
                                                         Type *Ty,
                                                         unsigned ID) {
  return new EZHConstantPoolConstant(Ty, GV, Offset, ID, EZHCP::CPConstant);
}

EZHConstantPoolConstant *EZHConstantPoolConstant::Create(const BlockAddress *BA,
                                                         unsigned ID) {
  return new EZHConstantPoolConstant(const_cast<Type *>(BA->getType()), BA, 0,
                                     ID, EZHCP::CPBlockAddress);
}

const GlobalValue *EZHConstantPoolConstant::getGlobalValue() const {
  return dyn_cast<GlobalValue>(CVal);
}

const BlockAddress *EZHConstantPoolConstant::getBlockAddress() const {
  return dyn_cast<BlockAddress>(CVal);
}

int EZHConstantPoolConstant::getExistingMachineCPValue(MachineConstantPool *CP,
                                                       Align Alignment) {
  return getExistingMachineCPValueImpl<EZHConstantPoolConstant>(CP, Alignment);
}

bool EZHConstantPoolConstant::hasSameValue(EZHConstantPoolValue *ACPV) {
  if (ACPV->getKind() == getKind() && ACPV->getLabelId() == getLabelId() &&
      ACPV->getType() == getType()) {
    return equals(cast<EZHConstantPoolConstant>(ACPV));
  }
  return false;
}

void EZHConstantPoolConstant::addSelectionDAGCSEId(FoldingSetNodeID &ID) {
  EZHConstantPoolValue::addSelectionDAGCSEId(ID);
  ID.AddPointer(CVal);
  ID.AddInteger(Offset);
}

void EZHConstantPoolConstant::print(raw_ostream &O) const {
  if (auto *GV = dyn_cast<GlobalValue>(CVal))
    O << "Global: " << GV->getName() << "+" << Offset;
  else if (auto *BA = dyn_cast<BlockAddress>(CVal))
    O << BA->getBasicBlock()->getName();
  else
    O << *CVal;
}

//===----------------------------------------------------------------------===//
// EZHConstantPoolSymbol
//===----------------------------------------------------------------------===//

EZHConstantPoolSymbol::EZHConstantPoolSymbol(Type *Ty, StringRef s, unsigned id)
    : EZHConstantPoolValue(Ty, id, EZHCP::CPExtSymbol), S(s) {}

EZHConstantPoolSymbol *EZHConstantPoolSymbol::Create(Type *Ty, StringRef s,
                                                     unsigned ID) {
  return new EZHConstantPoolSymbol(Ty, s, ID);
}

int EZHConstantPoolSymbol::getExistingMachineCPValue(MachineConstantPool *CP,
                                                     Align Alignment) {
  return getExistingMachineCPValueImpl<EZHConstantPoolSymbol>(CP, Alignment);
}

bool EZHConstantPoolSymbol::hasSameValue(EZHConstantPoolValue *ACPV) {
  if (ACPV->getKind() == getKind() && ACPV->getLabelId() == getLabelId() &&
      ACPV->getType() == getType()) {
    return equals(cast<EZHConstantPoolSymbol>(ACPV));
  }
  return false;
}

void EZHConstantPoolSymbol::addSelectionDAGCSEId(FoldingSetNodeID &ID) {
  EZHConstantPoolValue::addSelectionDAGCSEId(ID);
  ID.AddString(S);
}

void EZHConstantPoolSymbol::print(raw_ostream &O) const { O << S; }

//===----------------------------------------------------------------------===//
// EZHConstantPoolMBB
//===----------------------------------------------------------------------===//

EZHConstantPoolMBB::EZHConstantPoolMBB(Type *Ty, const MachineBasicBlock *mbb,
                                       unsigned id)
    : EZHConstantPoolValue(Ty, id, EZHCP::CPMachineBasicBlock), MBB(mbb) {}

EZHConstantPoolMBB *EZHConstantPoolMBB::Create(Type *Ty,
                                               const MachineBasicBlock *mbb,
                                               unsigned ID) {
  return new EZHConstantPoolMBB(Ty, mbb, ID);
}

int EZHConstantPoolMBB::getExistingMachineCPValue(MachineConstantPool *CP,
                                                  Align Alignment) {
  return getExistingMachineCPValueImpl<EZHConstantPoolMBB>(CP, Alignment);
}

bool EZHConstantPoolMBB::hasSameValue(EZHConstantPoolValue *ACPV) {
  if (ACPV->getKind() == getKind() && ACPV->getLabelId() == getLabelId() &&
      ACPV->getType() == getType()) {
    return equals(cast<EZHConstantPoolMBB>(ACPV));
  }
  return false;
}

void EZHConstantPoolMBB::addSelectionDAGCSEId(FoldingSetNodeID &ID) {
  EZHConstantPoolValue::addSelectionDAGCSEId(ID);
  ID.AddPointer(MBB);
}

void EZHConstantPoolMBB::print(raw_ostream &O) const {
  O << MBB->getSymbol()->getName();
}
