//===-- EZHConstantPoolValue.h - EZH Constant Pool Value ------*- C++ -*-===//
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
//   Defines EZH-specific constant pool value classes, representing
//   custom PC-relative displacement entries (GlobalValues, ExternalSymbols,
//   BlockAddresses, MachineBasicBlocks) for the constant island pass.
//
// Copied From:
//   ARM target backend (llvm/lib/Target/ARM/ARMConstantPoolValue.h).
//
// Changes:
//   Adapted references to EZH (EZHCP namespaces, EZHConstantPoolValue classes);
//   simplified to only support active EZH constant pool kinds.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EZH_EZHCONSTANTPOOLVALUE_H
#define LLVM_LIB_TARGET_EZH_EZHCONSTANTPOOLVALUE_H

#include "llvm/ADT/FoldingSet.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"
#include <string>
#include <vector>

namespace llvm {

class BlockAddress;
class GlobalValue;
class LLVMContext;
class MachineBasicBlock;
class raw_ostream;
class Type;

namespace EZHCP {

enum EZHCPKind { CPConstant, CPExtSymbol, CPBlockAddress, CPMachineBasicBlock };
} // end namespace EZHCP

/// EZHConstantPoolValue - EZH specific constant pool value base class.
class EZHConstantPoolValue : public MachineConstantPoolValue {
  unsigned LabelId;
  EZHCP::EZHCPKind Kind;

protected:
  EZHConstantPoolValue(Type *Ty, unsigned id, EZHCP::EZHCPKind Kind);

  template <typename Derived>
  int getExistingMachineCPValueImpl(MachineConstantPool *CP, Align Alignment) {
    const std::vector<MachineConstantPoolEntry> &Constants = CP->getConstants();
    for (unsigned i = 0, e = Constants.size(); i != e; ++i) {
      if (Constants[i].isMachineConstantPoolEntry() &&
          Constants[i].getAlign() >= Alignment) {
        auto *CPV =
            static_cast<EZHConstantPoolValue *>(Constants[i].Val.MachineCPVal);
        if (Derived *APC = dyn_cast<Derived>(CPV))
          if (cast<Derived>(this)->equals(APC))
            return i;
      }
    }

    return -1;
  }

public:
  ~EZHConstantPoolValue() override = default;

  unsigned getLabelId() const { return LabelId; }
  void setLabelId(unsigned ID) { LabelId = ID; }
  EZHCP::EZHCPKind getKind() const { return Kind; }

  bool isConstant() const { return Kind == EZHCP::CPConstant; }
  bool isExtSymbol() const { return Kind == EZHCP::CPExtSymbol; }
  bool isBlockAddress() const { return Kind == EZHCP::CPBlockAddress; }
  bool isMachineBasicBlock() const {
    return Kind == EZHCP::CPMachineBasicBlock;
  }
  bool isGlobalValue() const { return isConstant(); }

  int getExistingMachineCPValue(MachineConstantPool *CP,
                                Align Alignment) override = 0;

  void addSelectionDAGCSEId(FoldingSetNodeID &ID) override;

  /// hasSameValue - Return true if this EZH constpool value can share the same
  /// constantpool entry as another EZH constpool value.
  virtual bool hasSameValue(EZHConstantPoolValue *ACPV);

  bool equals(const EZHConstantPoolValue *A) const {
    return Kind == A->Kind && LabelId == A->LabelId &&
           MachineConstantPoolValue::getType() == A->getType();
  }

  void print(raw_ostream &O) const override = 0;

  static bool classof(const MachineConstantPoolValue *V) { return true; }
};

/// EZHConstantPoolConstant - EZH-specific constant pool values for Constants,
/// GlobalValues, and BlockAddresses.
class EZHConstantPoolConstant : public EZHConstantPoolValue {
  const Constant *CVal;
  int64_t Offset;

  EZHConstantPoolConstant(Type *Ty, const Constant *C, int64_t Offset,
                          unsigned ID, EZHCP::EZHCPKind Kind);

public:
  static EZHConstantPoolConstant *Create(const Constant *C, unsigned ID = 0);
  static EZHConstantPoolConstant *Create(const GlobalValue *GV, int64_t Offset,
                                         Type *Ty, unsigned ID = 0);
  static EZHConstantPoolConstant *Create(const BlockAddress *BA,
                                         unsigned ID = 0);

  const GlobalValue *getGlobalValue() const;
  const BlockAddress *getBlockAddress() const;
  const Constant *getConstant() const { return CVal; }
  int64_t getOffset() const { return Offset; }

  int getExistingMachineCPValue(MachineConstantPool *CP,
                                Align Alignment) override;

  /// hasSameValue - Return true if this EZH constpool value can share the same
  /// constantpool entry as another EZH constpool value.
  bool hasSameValue(EZHConstantPoolValue *ACPV) override;

  void addSelectionDAGCSEId(FoldingSetNodeID &ID) override;

  void print(raw_ostream &O) const override;

  static bool classof(const EZHConstantPoolValue *APV) {
    return APV->isConstant() || APV->isBlockAddress();
  }

  bool equals(const EZHConstantPoolConstant *A) const {
    return CVal == A->CVal && Offset == A->Offset &&
           EZHConstantPoolValue::equals(A);
  }
};

/// EZHConstantPoolSymbol - EZH-specific constant pool values for external
/// symbols.
class EZHConstantPoolSymbol : public EZHConstantPoolValue {
  const std::string S; // ExtSymbol being loaded.

  EZHConstantPoolSymbol(Type *Ty, StringRef s, unsigned id);

public:
  static EZHConstantPoolSymbol *Create(Type *Ty, StringRef s, unsigned ID = 0);

  StringRef getSymbol() const { return S; }

  int getExistingMachineCPValue(MachineConstantPool *CP,
                                Align Alignment) override;

  void addSelectionDAGCSEId(FoldingSetNodeID &ID) override;

  /// hasSameValue - Return true if this EZH constpool value can share the same
  /// constantpool entry as another EZH constpool value.
  bool hasSameValue(EZHConstantPoolValue *ACPV) override;

  void print(raw_ostream &O) const override;

  static bool classof(const EZHConstantPoolValue *ACPV) {
    return ACPV->isExtSymbol();
  }

  bool equals(const EZHConstantPoolSymbol *A) const {
    return S == A->S && EZHConstantPoolValue::equals(A);
  }
};

/// EZHConstantPoolMBB - EZH-specific constant pool value of a machine basic
/// block.
class EZHConstantPoolMBB : public EZHConstantPoolValue {
  const MachineBasicBlock *MBB; // Machine basic block.

  EZHConstantPoolMBB(Type *Ty, const MachineBasicBlock *mbb, unsigned id);

public:
  static EZHConstantPoolMBB *Create(Type *Ty, const MachineBasicBlock *mbb,
                                    unsigned ID = 0);

  const MachineBasicBlock *getMBB() const { return MBB; }

  int getExistingMachineCPValue(MachineConstantPool *CP,
                                Align Alignment) override;

  void addSelectionDAGCSEId(FoldingSetNodeID &ID) override;

  /// hasSameValue - Return true if this EZH constpool value can share the same
  /// constantpool entry as another EZH constpool value.
  bool hasSameValue(EZHConstantPoolValue *ACPV) override;

  void print(raw_ostream &O) const override;

  static bool classof(const EZHConstantPoolValue *ACPV) {
    return ACPV->isMachineBasicBlock();
  }

  bool equals(const EZHConstantPoolMBB *A) const {
    return MBB == A->MBB && EZHConstantPoolValue::equals(A);
  }
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EZH_EZHCONSTANTPOOLVALUE_H
