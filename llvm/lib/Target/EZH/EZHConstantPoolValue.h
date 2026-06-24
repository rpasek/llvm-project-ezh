//===- EZHConstantPoolValue.h - EZH constantpool value ----------*- C++ -*-===//
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

#ifndef LLVM_LIB_TARGET_EZH_EZHCONSTANTPOOLVALUE_H
#define LLVM_LIB_TARGET_EZH_EZHCONSTANTPOOLVALUE_H

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/Support/Casting.h"
#include <string>
#include <vector>

namespace llvm {

class BlockAddress;
class Constant;
class GlobalValue;
class GlobalVariable;
class LLVMContext;
class MachineBasicBlock;
class raw_ostream;
class Type;

namespace EZHCP {

enum EZHCPKind {
  CPValue,
  CPExtSymbol,
  CPBlockAddress,
  CPLSDA,
  CPMachineBasicBlock,
  CPPromotedGlobal
};

enum EZHCPModifier {
  no_modifier, /// None
  TLSGD,       /// Thread Local Storage (General Dynamic Mode)
  GOT_PREL,    /// Global Offset Table, PC Relative
  GOTTPOFF,    /// Global Offset Table, Thread Pointer Offset
  TPOFF,       /// Thread Pointer Offset
  SECREL,      /// Section Relative (Windows TLS)
  SBREL,       /// Static Base Relative (RWPI)
};

} // end namespace EZHCP

/// EZHConstantPoolValue - EZH specific constantpool value. This is used to
/// represent PC-relative displacement between the address of the load
/// instruction and the constant being loaded, i.e. (&GV-(LPIC+8)).
class EZHConstantPoolValue : public MachineConstantPoolValue {
  unsigned LabelId;       // Label id of the load.
  EZHCP::EZHCPKind Kind;  // Kind of constant.
  unsigned char PCAdjust; // Extra adjustment if constantpool is pc-relative.
                          // 8 for EZH, 4 for Thumb.
  EZHCP::EZHCPModifier Modifier; // GV modifier i.e. (&GV(modifier)-(LPIC+8))
  bool AddCurrentAddress;

protected:
  EZHConstantPoolValue(Type *Ty, unsigned id, EZHCP::EZHCPKind Kind,
                       unsigned char PCAdj, EZHCP::EZHCPModifier Modifier,
                       bool AddCurrentAddress);

  EZHConstantPoolValue(LLVMContext &C, unsigned id, EZHCP::EZHCPKind Kind,
                       unsigned char PCAdj, EZHCP::EZHCPModifier Modifier,
                       bool AddCurrentAddress);

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
  ~EZHConstantPoolValue() override;

  EZHCP::EZHCPModifier getModifier() const { return Modifier; }
  StringRef getModifierText() const;
  bool hasModifier() const { return Modifier != EZHCP::no_modifier; }

  bool mustAddCurrentAddress() const { return AddCurrentAddress; }

  unsigned getLabelId() const { return LabelId; }
  unsigned char getPCAdjustment() const { return PCAdjust; }

  bool isGlobalValue() const { return Kind == EZHCP::CPValue; }
  bool isExtSymbol() const { return Kind == EZHCP::CPExtSymbol; }
  bool isBlockAddress() const { return Kind == EZHCP::CPBlockAddress; }
  bool isLSDA() const { return Kind == EZHCP::CPLSDA; }
  bool isMachineBasicBlock() const {
    return Kind == EZHCP::CPMachineBasicBlock;
  }
  bool isPromotedGlobal() const { return Kind == EZHCP::CPPromotedGlobal; }

  int getExistingMachineCPValue(MachineConstantPool *CP,
                                Align Alignment) override;

  void addSelectionDAGCSEId(FoldingSetNodeID &ID) override;

  /// hasSameValue - Return true if this EZH constpool value can share the same
  /// constantpool entry as another EZH constpool value.
  virtual bool hasSameValue(EZHConstantPoolValue *ACPV);

  bool equals(const EZHConstantPoolValue *A) const {
    return this->LabelId == A->LabelId && this->PCAdjust == A->PCAdjust &&
           this->Modifier == A->Modifier;
  }

  void print(raw_ostream &O) const override;
  void print(raw_ostream *O) const {
    if (O)
      print(*O);
  }
  void dump() const;
};

inline raw_ostream &operator<<(raw_ostream &O, const EZHConstantPoolValue &V) {
  V.print(O);
  return O;
}

/// EZHConstantPoolConstant - EZH-specific constant pool values for Constants,
/// Functions, and BlockAddresses.
class EZHConstantPoolConstant : public EZHConstantPoolValue {
  const Constant *CVal; // Constant being loaded.
  SmallPtrSet<const GlobalVariable *, 1> GVars;

  EZHConstantPoolConstant(const Constant *C, unsigned ID, EZHCP::EZHCPKind Kind,
                          unsigned char PCAdj, EZHCP::EZHCPModifier Modifier,
                          bool AddCurrentAddress);
  EZHConstantPoolConstant(Type *Ty, const Constant *C, unsigned ID,
                          EZHCP::EZHCPKind Kind, unsigned char PCAdj,
                          EZHCP::EZHCPModifier Modifier,
                          bool AddCurrentAddress);
  EZHConstantPoolConstant(const GlobalVariable *GV, const Constant *Init);

public:
  static EZHConstantPoolConstant *Create(const Constant *C, unsigned ID);
  static EZHConstantPoolConstant *Create(const GlobalValue *GV,
                                         EZHCP::EZHCPModifier Modifier);
  static EZHConstantPoolConstant *Create(const GlobalVariable *GV,
                                         const Constant *Initializer);
  static EZHConstantPoolConstant *Create(const Constant *C, unsigned ID,
                                         EZHCP::EZHCPKind Kind,
                                         unsigned char PCAdj);
  static EZHConstantPoolConstant *Create(const Constant *C, unsigned ID,
                                         EZHCP::EZHCPKind Kind,
                                         unsigned char PCAdj,
                                         EZHCP::EZHCPModifier Modifier,
                                         bool AddCurrentAddress);

  const GlobalValue *getGV() const;
  const BlockAddress *getBlockAddress() const;

  using promoted_iterator = SmallPtrSet<const GlobalVariable *, 1>::iterator;

  iterator_range<promoted_iterator> promotedGlobals() { return GVars; }

  const Constant *getPromotedGlobalInit() const { return CVal; }

  int getExistingMachineCPValue(MachineConstantPool *CP,
                                Align Alignment) override;

  /// hasSameValue - Return true if this EZH constpool value can share the same
  /// constantpool entry as another EZH constpool value.
  bool hasSameValue(EZHConstantPoolValue *ACPV) override;

  void addSelectionDAGCSEId(FoldingSetNodeID &ID) override;

  void print(raw_ostream &O) const override;

  static bool classof(const EZHConstantPoolValue *APV) {
    return APV->isGlobalValue() || APV->isBlockAddress() || APV->isLSDA() ||
           APV->isPromotedGlobal();
  }

  bool equals(const EZHConstantPoolConstant *A) const {
    return CVal == A->CVal && EZHConstantPoolValue::equals(A);
  }
};

/// EZHConstantPoolSymbol - EZH-specific constantpool values for external
/// symbols.
class EZHConstantPoolSymbol : public EZHConstantPoolValue {
  const std::string S; // ExtSymbol being loaded.

  EZHConstantPoolSymbol(LLVMContext &C, StringRef s, unsigned id,
                        unsigned char PCAdj, EZHCP::EZHCPModifier Modifier,
                        bool AddCurrentAddress);

public:
  static EZHConstantPoolSymbol *Create(LLVMContext &C, StringRef s, unsigned ID,
                                       unsigned char PCAdj);

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

/// EZHConstantPoolMBB - EZH-specific constantpool value of a machine basic
/// block.
class EZHConstantPoolMBB : public EZHConstantPoolValue {
  const MachineBasicBlock *MBB; // Machine basic block.

  EZHConstantPoolMBB(LLVMContext &C, const MachineBasicBlock *mbb, unsigned id,
                     unsigned char PCAdj, EZHCP::EZHCPModifier Modifier,
                     bool AddCurrentAddress);

public:
  static EZHConstantPoolMBB *Create(LLVMContext &C,
                                    const MachineBasicBlock *mbb, unsigned ID,
                                    unsigned char PCAdj);

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
