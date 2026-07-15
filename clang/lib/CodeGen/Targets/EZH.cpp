//===- EZH.cpp ------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// EZH ABI Implementation
//
// This ABI implementation is copied and simplified from the ARM AAPCS
// implementation in clang/lib/CodeGen/Targets/ARM.cpp.
//
// EZH is a 32-bit architecture that uses a calling convention similar to
// Thumb1 (subset of AAPCS soft-float).
//
// Differences from standard ARM AAPCS:
// - EZH has different DWARF register mappings:
//   - Stack Pointer (SP) is mapped to DWARF register 12 (ARM uses 13).
// - Stripped of VFP, NEON, MVE, Swift ABI, and Windows/WatchOS exceptions.
//===----------------------------------------------------------------------===//

#include "ABIInfoImpl.h"
#include "TargetInfo.h"

using namespace clang;
using namespace clang::CodeGen;

namespace {
class EZHABIInfo : public DefaultABIInfo {
public:
  EZHABIInfo(CodeGen::CodeGenTypes &CGT) : DefaultABIInfo(CGT) {}

  void computeInfo(CGFunctionInfo &FI) const override;
  ABIArgInfo classifyArgumentType(QualType Ty) const;
  ABIArgInfo classifyReturnType(QualType RetTy) const;
  RValue EmitVAArg(CodeGenFunction &CGF, Address VAListAddr, QualType Ty,
                   AggValueSlot Slot) const override;
  bool isIllegalVectorType(QualType Ty) const;
  ABIArgInfo coerceIllegalVector(QualType Ty) const;
};
} // end anonymous namespace

void EZHABIInfo::computeInfo(CGFunctionInfo &FI) const {
  if (!getCXXABI().classifyReturnType(FI))
    FI.getReturnInfo() = classifyReturnType(FI.getReturnType());

  for (auto &I : FI.arguments())
    I.info = classifyArgumentType(I.type);
}

RValue EZHABIInfo::EmitVAArg(CodeGenFunction &CGF, Address VAListAddr,
                             QualType Ty, AggValueSlot Slot) const {
  CharUnits SlotSize = CharUnits::fromQuantity(4);

  // Empty records are ignored for parameter passing purposes.
  uint64_t Size = getContext().getTypeSize(Ty);
  bool IsEmpty = isEmptyRecord(getContext(), Ty, true);
  if ((IsEmpty || Size == 0) && (!getContext().getLangOpts().CPlusPlus))
    return Slot.asRValue();

  CharUnits TySize = getContext().getTypeSizeInChars(Ty);

  // The EZH stack is only 4-byte aligned (datalayout S32) and CC_EZH packs
  // byval arguments at 4 (CCPassByVal<4, 4>), so no vararg slot can be
  // assumed more aligned than 4 regardless of the type's declared
  // alignment: rounding ap up to 8 would skip a real argument word
  // whenever the caller's stack pointer happened to sit at 4 mod 8, which
  // depends on nothing more stable than the caller's register pressure.
  CharUnits TyAlignForABI = CharUnits::fromQuantity(4);

  TypeInfoChars TyInfo(TySize, TyAlignForABI, AlignRequirementKind::None);
  return emitVoidPtrVAArg(CGF, VAListAddr, Ty, /*IsIndirect=*/false, TyInfo,
                          SlotSize, /*AllowHigherAlign=*/true, Slot);
}

bool EZHABIInfo::isIllegalVectorType(QualType Ty) const {
  return Ty->isVectorType();
}

ABIArgInfo EZHABIInfo::coerceIllegalVector(QualType Ty) const {
  uint64_t Size = getContext().getTypeSize(Ty);
  if (Size <= 32)
    return ABIArgInfo::getDirect(llvm::Type::getInt32Ty(getVMContext()));
  if (Size <= 64)
    return ABIArgInfo::getDirect(llvm::Type::getInt64Ty(getVMContext()));
  return getNaturalAlignIndirect(Ty, /*AddrSpace=*/getDataLayout().getAllocaAddrSpace(),
                                 /*ByVal=*/false);
}

ABIArgInfo EZHABIInfo::classifyArgumentType(QualType Ty) const {
  Ty = useFirstFieldIfTransparentUnion(Ty);

  if (isIllegalVectorType(Ty))
    return coerceIllegalVector(Ty);

  if (!isAggregateTypeForABI(Ty)) {
    // Treat an enum type as its underlying type.
    if (const auto *ED = Ty->getAsEnumDecl())
      Ty = ED->getIntegerType();

    // Use default classification for scalars.
    return DefaultABIInfo::classifyArgumentType(Ty);
  }

  if (CGCXXABI::RecordArgABI RAA = getRecordArgABI(Ty, getCXXABI())) {
    return getNaturalAlignIndirect(Ty, /*AddrSpace=*/getDataLayout().getAllocaAddrSpace(),
                                   RAA == CGCXXABI::RAA_DirectInMemory);
  }

  if (isEmptyRecord(getContext(), Ty, true))
    return ABIArgInfo::getIgnore();

  // AAPCS rules for aggregate alignment and size.
  // The ABI alignment for AAPCS is at least 4-byte and at most 8-byte.
  // We realign the indirect argument if type alignment is bigger than ABI alignment.
  uint64_t TyAlign = getContext().getTypeUnadjustedAlignInChars(Ty).getQuantity();
  uint64_t ABIAlign = std::clamp(TyAlign, (uint64_t)4, (uint64_t)8);

  // If size > 64 bytes, pass indirect byval.
  if (getContext().getTypeSizeInChars(Ty) > CharUnits::fromQuantity(64)) {
    return ABIArgInfo::getIndirect(
        CharUnits::fromQuantity(ABIAlign),
        /*AddrSpace=*/getDataLayout().getAllocaAddrSpace(),
        /*ByVal=*/true, /*Realign=*/TyAlign > ABIAlign);
  }

  // Otherwise, pass by coercing to a structure of the appropriate size.
  llvm::Type* ElemTy;
  unsigned SizeRegs;
  if (TyAlign <= 4) {
    ElemTy = llvm::Type::getInt32Ty(getVMContext());
    SizeRegs = (getContext().getTypeSize(Ty) + 31) / 32;
  } else {
    ElemTy = llvm::Type::getInt64Ty(getVMContext());
    SizeRegs = (getContext().getTypeSize(Ty) + 63) / 64;
  }

  return ABIArgInfo::getDirect(llvm::ArrayType::get(ElemTy, SizeRegs));
}

ABIArgInfo EZHABIInfo::classifyReturnType(QualType RetTy) const {
  if (RetTy->isVoidType())
    return ABIArgInfo::getIgnore();

  if (isIllegalVectorType(RetTy))
    return coerceIllegalVector(RetTy);

  if (!isAggregateTypeForABI(RetTy)) {
    // Treat an enum type as its underlying type.
    if (const auto *ED = RetTy->getAsEnumDecl())
      RetTy = ED->getIntegerType();

    // Use default classification for scalars.
    return DefaultABIInfo::classifyReturnType(RetTy);
  }

  if (isEmptyRecord(getContext(), RetTy, true) ||
      getContext().getTypeSize(RetTy) == 0)
    return ABIArgInfo::getIgnore();

  // Aggregates <= 4 bytes are returned in r0; other aggregates
  // are returned indirectly.
  uint64_t Size = getContext().getTypeSize(RetTy);
  if (Size <= 32) {
    // Return in the smallest viable integer type.
    if (Size <= 8)
      return ABIArgInfo::getDirect(llvm::Type::getInt8Ty(getVMContext()));
    if (Size <= 16)
      return ABIArgInfo::getDirect(llvm::Type::getInt16Ty(getVMContext()));
    return ABIArgInfo::getDirect(llvm::Type::getInt32Ty(getVMContext()));
  }

  return getNaturalAlignIndirect(RetTy, getDataLayout().getAllocaAddrSpace());
}

namespace {
class EZHTargetCodeGenInfo : public TargetCodeGenInfo {
public:
  EZHTargetCodeGenInfo(CodeGen::CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<EZHABIInfo>(CGT)) {}

  int getDwarfEHStackPointer(CodeGen::CodeGenModule &M) const override {
    return 12; // EZH SP is 12
  }

  bool initDwarfEHRegSizeTable(CodeGen::CodeGenFunction &CGF,
                               llvm::Value *Address) const override {
    llvm::Value *Four8 = llvm::ConstantInt::get(CGF.Int8Ty, 4);
    // EZH has 16 registers (0-15).
    AssignToArrayRange(CGF.Builder, Address, Four8, 0, 15);
    return false;
  }
};
} // end anonymous namespace

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createEZHTargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<EZHTargetCodeGenInfo>(CGM.getTypes());
}
