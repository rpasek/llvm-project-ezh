//===-- EZHISelLowering.h - EZH DAG Lowering Interface -....-*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that EZH uses to lower LLVM code into a
// selection DAG.
//
// Copied From:
//   Lanai target backend (llvm/lib/Target/Lanai/LanaiISelLowering.h).
//
// Changes:
//   Renamed LanaiTargetLowering to EZHTargetLowering; configured operation
//   legalization actions (promoting 8/16-bit ops, expanding unsupported
//   comparisons); declared custom lowering for arguments, returns, calls, and
//   comparisons.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EZH_EZHISELLOWERING_H
#define LLVM_LIB_TARGET_EZH_EZHISELLOWERING_H

#include "EZH.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLowering.h"
#include <optional>
#include <utility>
#include <vector>

namespace llvm {

namespace EZHISD {
enum NodeType : unsigned {
  FIRST_NUMBER = ISD::BUILTIN_OP_END,
  PER_READ,
  PER_WRITE,
  RET_GLUE_INTERNAL,
  CALL,
  CMP,
  BR_CC,
  SELECT_CC,
  BTOG,
  BR_JT,
  EH_SJLJ_SETJMP,
  EH_SJLJ_LONGJMP,
  EH_SJLJ_SETUP_DISPATCH,
};
} // namespace EZHISD

class EZHSubtarget;
struct EZHRegisterInfo;

class EZHTargetLowering : public TargetLowering {
public:
  EZHTargetLowering(const TargetMachine &TM, const EZHSubtarget &STI);

  // LowerOperation - Provide custom lowering hooks for some operations.
  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;

  SDValue LowerBlockAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerBR_CC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerBR_JT(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerConstant(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerConstantPool(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerDYNAMIC_STACKALLOC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerFRAMEADDR(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerINTRINSIC_WO_CHAIN(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerJumpTable(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerSELECT_CC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerVAARG(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerVASTART(SDValue Op, SelectionDAG &DAG) const;

  void ReplaceNodeResults(SDNode *N, SmallVectorImpl<SDValue> &Results,
                          SelectionDAG &DAG) const override;

  bool CanLowerReturn(CallingConv::ID CallConv, MachineFunction &MF,
                      bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      LLVMContext &Context, const Type *RetTy) const override;

  Register getRegisterByName(const char *RegName, LLT Ty,
                             const MachineFunction &MF) const override;

  Register
  getExceptionPointerRegister(const Constant *PersonalityFn) const override;
  Register
  getExceptionSelectorRegister(const Constant *PersonalityFn) const override;

  std::pair<unsigned, const TargetRegisterClass *>
  getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                               StringRef Constraint, MVT VT) const override;

  const char *getTargetNodeName(unsigned Opcode) const override;
  MachineBasicBlock *
  EmitInstrWithCustomInserter(MachineInstr &MI,
                              MachineBasicBlock *BB) const override;
  MachineBasicBlock *emitEHSjLjSetJmp(MachineInstr &MI,
                                      MachineBasicBlock *MBB) const;
  MachineBasicBlock *emitEHSjLjLongJmp(MachineInstr &MI,
                                       MachineBasicBlock *MBB) const;
  MachineBasicBlock *emitSjLjDispatchBlock(MachineInstr &MI,
                                           MachineBasicBlock *MBB) const;

  EVT getSetCCResultType(const DataLayout &DL, LLVMContext &Context,
                         EVT VT) const override;

  bool decomposeMulByConstant(LLVMContext &Context, EVT VT,
                              SDValue C) const override;

  bool getPostIndexedAddressParts(SDNode *N, SDNode *Op, SDValue &Base,
                                  SDValue &Offset, ISD::MemIndexedMode &AM,
                                  SelectionDAG &DAG) const override;

  bool getPreIndexedAddressParts(SDNode *N, SDValue &Base, SDValue &Offset,
                                 ISD::MemIndexedMode &AM,
                                 SelectionDAG &DAG) const override;

  bool shouldReduceLoadWidth(
      SDNode *Load, ISD::LoadExtType ExtTy, EVT NewVT,
      std::optional<unsigned> ByteOffset = std::nullopt) const override;

private:
  SDValue LowerCallResult(SDValue Chain, SDValue InGlue,
                          CallingConv::ID CallConv, bool IsVarArg,
                          const SmallVectorImpl<ISD::InputArg> &Ins,
                          const SDLoc &DL, SelectionDAG &DAG,
                          SmallVectorImpl<SDValue> &InVals) const;

  SDValue LowerCall(TargetLowering::CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;

  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                               bool IsVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &DL, SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;

  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
                      SelectionDAG &DAG) const override;

  bool getIndexedAddressParts(SDNode *N, SDValue Ptr, SDValue &Base,
                              SDValue &Offset, ISD::MemIndexedMode &AM,
                              bool IsPre, SelectionDAG &DAG) const;
};
} // namespace llvm

#endif // LLVM_LIB_TARGET_EZH_EZHISELLOWERING_H
