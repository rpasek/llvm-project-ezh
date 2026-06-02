//===-- EZHTargetTransformInfo.h - EZH specific TTI ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines a TargetTransformInfoImplBase conforming object specific
// to the EZH target machine. It uses the target's detailed information to
// provide more precise answers to certain TTI queries, while letting the
// target independent and default TTI implementations handle the rest.
//
// Description:
//   Implements EZHTIImpl, providing target-specific cost model queries for
//   vectorization, unrolling, and IR transformations.
//
// Copied From:
//   Lanai target backend (llvm/lib/Target/Lanai/LanaiTargetTransformInfo.h).
//
// Changes:
//   Renamed LanaiTTIImpl to EZHTIImpl; configured instruction cost estimates
//   reflecting EZH's lack of cache and specific 8/32-bit memory access costs.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EZH_EZHTARGETTRANSFORMINFO_H
#define LLVM_LIB_TARGET_EZH_EZHTARGETTRANSFORMINFO_H

#include "EZH.h"
#include "EZHSubtarget.h"
#include "EZHTargetMachine.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/Support/MathExtras.h"

namespace llvm {
class EZHTTIImpl final : public BasicTTIImplBase<EZHTTIImpl> {
  typedef BasicTTIImplBase<EZHTTIImpl> BaseT;
  typedef TargetTransformInfo TTI;
  friend BaseT;

  const EZHSubtarget *ST;
  const EZHTargetLowering *TLI;

  const EZHSubtarget *getST() const { return ST; }
  const EZHTargetLowering *getTLI() const { return TLI; }

public:
  explicit EZHTTIImpl(const EZHTargetMachine *TM, const Function &F)
      : BaseT(TM, F.getDataLayout()), ST(TM->getSubtargetImpl(F)),
        TLI(ST->getTargetLowering()) {}

  InstructionCost getIntImmCost(const APInt &Imm, Type *Ty,
                                TTI::TargetCostKind CostKind) const override {
    assert(Ty->isIntegerTy() && "Expected integer type!");
    unsigned BitSize = Ty->getPrimitiveSizeInBits();
    if (BitSize == 0 || Imm.getActiveBits() >= 64)
      return TTI::TCC_Expensive;

    int64_t Val = Imm.getSExtValue();
    if (Val == 0)
      return TTI::TCC_Free;

    // Fits natively in EZH's 11-bit signed immediate (e_load_simm)
    if (isInt<11>(Val))
      return TTI::TCC_Basic;

    // Fits in EZH's shifted 11-bit immediate (e_load_simm with shift)
    uint64_t UVal = Imm.getZExtValue();
    unsigned TZ = llvm::countr_zero(UVal);
    if (isInt<11>(static_cast<int32_t>(UVal >> TZ)))
      return TTI::TCC_Basic;

    // Fallback: Requires a constant pool load (expensive memory access)
    return 2 * TTI::TCC_Basic;
  }

  InstructionCost
  getIntImmCostInst(unsigned Opc, unsigned Idx, const APInt &Imm, Type *Ty,
                    TTI::TargetCostKind CostKind,
                    Instruction *Inst = nullptr) const override {
    return getIntImmCost(Imm, Ty, CostKind);
  }

  InstructionCost
  getIntImmCostIntrin(Intrinsic::ID IID, unsigned Idx, const APInt &Imm,
                      Type *Ty, TTI::TargetCostKind CostKind) const override {
    return getIntImmCost(Imm, Ty, CostKind);
  }

  InstructionCost getArithmeticInstrCost(
      unsigned Opcode, Type *Ty, TTI::TargetCostKind CostKind,
      TTI::OperandValueInfo Op1Info = {TTI::OK_AnyValue, TTI::OP_None},
      TTI::OperandValueInfo Op2Info = {TTI::OK_AnyValue, TTI::OP_None},
      ArrayRef<const Value *> Args = {},
      const Instruction *CxtI = nullptr) const override {
    // EZH has no hardware floating-point unit; FP operations expand to runtime
    // soft-float library calls (__addsf3, __mulsf3, etc.).
    if (Ty && Ty->isFloatingPointTy())
      return 64;

    int ISD = TLI->InstructionOpcodeToISD(Opcode);

    switch (ISD) {
    default:
      return BaseT::getArithmeticInstrCost(Opcode, Ty, CostKind, Op1Info,
                                           Op2Info);
    case ISD::MUL:
    case ISD::SDIV:
    case ISD::UDIV:
    case ISD::SREM:
    case ISD::UREM:
      // Penalize software-emulated multiplication and division.
      return 64 * BaseT::getArithmeticInstrCost(Opcode, Ty, CostKind, Op1Info,
                                                Op2Info);
    }
  }

  InstructionCost
  getCmpSelInstrCost(unsigned Opcode, Type *ValTy, Type *CondTy,
                     CmpInst::Predicate Pred, TTI::TargetCostKind CostKind,
                     const Instruction *I = nullptr) const override {
    if (Opcode == Instruction::Select) {
      // EZH supports conditional moves / predicated instruction execution.
      return TTI::TCC_Basic;
    }
    if (Opcode == Instruction::ICmp && CmpInst::isSigned(Pred) && ValTy) {
      unsigned BitWidth = ValTy->getScalarSizeInBits();
      if (BitWidth > 16) {
        // EZH lacks an ALU overflow flag; 32-bit and 64-bit signed comparisons
        // expand into multi-instruction sequences checking borrow/sign flags.
        return 2 * TTI::TCC_Basic;
      }
    }
    return BaseT::getCmpSelInstrCost(Opcode, ValTy, CondTy, Pred, CostKind, I);
  }
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_EZH_EZHTARGETTRANSFORMINFO_H
