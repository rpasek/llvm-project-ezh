//===-- EZHISelDAGToDAG.cpp - A dag to dag inst selector for EZH ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the EZH target.
//
// Description:
//   Implements EZHDAGToDAGISel, the primary instruction selector pattern-
//   matching SelectionDAG nodes into EZH machine instructions.
//
// Copied From:
//   Lanai target backend (llvm/lib/Target/Lanai/LanaiISelDAGToDAG.cpp).
//
// Changes:
//   Adapted Lanai ISel to EZH; implemented custom selection methods for
//   indexed load/store instructions (tryIndexedLoadStore) and inline assembly
//   memory operands matching EZH addressing modes.
//
//===----------------------------------------------------------------------===//

#include "EZHCondCode.h"
#include "EZHTargetMachine.h"
#include "MCTargetDesc/EZHMCTargetDesc.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "ezh-isel"
#define PASS_NAME "EZH DAG->DAG Pattern Instruction Selection"

namespace {

class EZHDAGToDAGISel : public SelectionDAGISel {
public:
  EZHDAGToDAGISel() = delete;

  explicit EZHDAGToDAGISel(EZHTargetMachine &TargetMachine)
      : SelectionDAGISel(TargetMachine) {}

private:
#include "EZHGenDAGISel.inc"

  void Select(SDNode *N) override;
  bool SelectInlineAsmMemoryOperand(const SDValue &Op,
                                    InlineAsm::ConstraintCode ConstraintID,
                                    std::vector<SDValue> &OutOps) override;
  void selectFrameIndex(SDNode *N);
  bool SelectFrameAddr(SDValue Addr, SDValue &Base, SDValue &Offset);
  bool tryIndexedLoadStore(SDNode *Node);
};

class EZHDAGToDAGISelLegacy : public SelectionDAGISelLegacy {
public:
  inline static char ID = 0;
  explicit EZHDAGToDAGISelLegacy(EZHTargetMachine &TM)
      : SelectionDAGISelLegacy(ID, std::make_unique<EZHDAGToDAGISel>(TM)) {}
};

} // namespace

INITIALIZE_PASS(EZHDAGToDAGISelLegacy, DEBUG_TYPE, PASS_NAME, false, false)

bool EZHDAGToDAGISel::tryIndexedLoadStore(SDNode *Node) {
  unsigned Opcode = Node->getOpcode();
  bool IsLoad = (Opcode == ISD::LOAD);
  ISD::MemIndexedMode AM;
  SDValue Base, Offset;

  if (IsLoad) {
    LoadSDNode *LD = cast<LoadSDNode>(Node);
    AM = LD->getAddressingMode();
    Base = LD->getBasePtr();
    Offset = LD->getOffset();
  } else {
    StoreSDNode *ST = cast<StoreSDNode>(Node);
    AM = ST->getAddressingMode();
    Base = ST->getBasePtr();
    Offset = ST->getOffset();
  }

  if (AM != ISD::POST_INC && AM != ISD::PRE_INC && AM != ISD::POST_DEC &&
      AM != ISD::PRE_DEC)
    return false;

  MemSDNode *MemNode = cast<MemSDNode>(Node);
  SDLoc DL(Node);
  EVT MemVT = MemNode->getMemoryVT();

  auto *C = dyn_cast<ConstantSDNode>(Offset);
  if (!C)
    return false;
  int64_t OffImm = C->getSExtValue();
  if (AM == ISD::POST_DEC || AM == ISD::PRE_DEC) {
    OffImm = -OffImm;
  }

  unsigned TargetOpcode = 0;

  if (MemVT == MVT::i32) {
    if ((OffImm & 3) != 0 || OffImm < -512 || OffImm > 508)
      return false;
    if (IsLoad) {
      TargetOpcode = (AM == ISD::POST_INC || AM == ISD::POST_DEC)
                         ? EZH::LDR_POST
                         : EZH::LDR_PRE;
    } else {
      TargetOpcode = (AM == ISD::POST_INC || AM == ISD::POST_DEC)
                         ? EZH::STR_POST
                         : EZH::STR_PRE;
    }
  } else if (MemVT == MVT::i8) {
    if (OffImm < -128 || OffImm > 255)
      return false;
    if (IsLoad) {
      if (cast<LoadSDNode>(Node)->getExtensionType() == ISD::SEXTLOAD) {
        TargetOpcode = (AM == ISD::POST_INC || AM == ISD::POST_DEC)
                           ? EZH::LDRBS_POST
                           : EZH::LDRBS_PRE;
      } else {
        TargetOpcode = (AM == ISD::POST_INC || AM == ISD::POST_DEC)
                           ? EZH::LDRB_POST
                           : EZH::LDRB_PRE;
      }
    } else {
      TargetOpcode = (AM == ISD::POST_INC || AM == ISD::POST_DEC)
                         ? EZH::STRB_POST
                         : EZH::STRB_PRE;
    }
  } else {
    return false; // i16 is not supported
  }

  SDValue TargetImm =
      CurDAG->getTargetConstant(static_cast<uint32_t>(OffImm), DL, MVT::i32);

  if (IsLoad) {
    SDValue Pred = CurDAG->getTargetConstant(EZHCC::ICC_EU, DL, MVT::i32);
    SDValue Ops[] = {Base, TargetImm, Pred, MemNode->getChain()};
    SDNode *ResNode = CurDAG->getMachineNode(
        TargetOpcode, DL, CurDAG->getVTList(MVT::i32, MVT::i32, MVT::Other),
        Ops);
    // Carry the load's MachineMemOperand so machine AA / scheduling can reason
    // about this access (the indexed instruction is mayLoad but has no pattern).
    CurDAG->setNodeMemRefs(cast<MachineSDNode>(ResNode),
                           {MemNode->getMemOperand()});
    ReplaceUses(SDValue(Node, 0), SDValue(ResNode, 0)); // Value
    ReplaceUses(SDValue(Node, 1), SDValue(ResNode, 1)); // New Ptr
    ReplaceUses(SDValue(Node, 2), SDValue(ResNode, 2)); // Chain
    CurDAG->RemoveDeadNode(Node);
  } else {
    SDValue Val = cast<StoreSDNode>(Node)->getValue();
    SDValue Pred = CurDAG->getTargetConstant(EZHCC::ICC_EU, DL, MVT::i32);
    SDValue Ops[] = {Val, Base, TargetImm, Pred, MemNode->getChain()};
    SDNode *ResNode = CurDAG->getMachineNode(
        TargetOpcode, DL, CurDAG->getVTList(MVT::i32, MVT::Other), Ops);
    // Carry the store's MachineMemOperand so machine AA / scheduling can reason
    // about this access (the indexed instruction is mayStore but has no pattern).
    CurDAG->setNodeMemRefs(cast<MachineSDNode>(ResNode),
                           {MemNode->getMemOperand()});
    ReplaceUses(SDValue(Node, 0), SDValue(ResNode, 0)); // New Ptr
    ReplaceUses(SDValue(Node, 1), SDValue(ResNode, 1)); // Chain
    CurDAG->RemoveDeadNode(Node);
  }

  return true;
}

bool EZHDAGToDAGISel::SelectInlineAsmMemoryOperand(
    const SDValue &Op, InlineAsm::ConstraintCode ConstraintID,
    std::vector<SDValue> &OutOps) {
  OutOps.push_back(Op);
  return false;
}

void EZHDAGToDAGISel::Select(SDNode *Node) {
  if (Node->isMachineOpcode()) {
    Node->setNodeId(-1);
    return;
  }

  if (Node->getOpcode() == ISD::CTLZ ||
      Node->getOpcode() == ISD::CTLZ_ZERO_POISON) {
    SDLoc dl(Node);
    SDValue Src = Node->getOperand(0);

    const TargetLowering &TLI = CurDAG->getTargetLoweringInfo();

    // Select appropriate CLZ library helper based on operand size
    EVT VT = Src.getValueType();
    const char *LibcallName = (VT == MVT::i64) ? "__clzdi2" : "__clzsi2";
    SDValue Callee = CurDAG->getExternalSymbol(
        LibcallName, TLI.getPointerTy(CurDAG->getDataLayout()));

    SDValue Chain = CurDAG->getEntryNode();
    SDValue InGlue;

    if (VT == MVT::i64) {
      // Explicitly generate EXTRACT_SUBREG nodes to let register allocator
      // handle virtual registers correctly!
      SDValue SrcLo =
          CurDAG->getNode(TargetOpcode::EXTRACT_SUBREG, dl, MVT::i32, Src,
                          CurDAG->getTargetConstant(sub_even, dl, MVT::i32));
      SDValue SrcHi =
          CurDAG->getNode(TargetOpcode::EXTRACT_SUBREG, dl, MVT::i32, Src,
                          CurDAG->getTargetConstant(sub_odd, dl, MVT::i32));

      Chain = CurDAG->getCopyToReg(Chain, dl, EZH::R0, SrcLo, InGlue);
      InGlue = Chain.getValue(1);
      Chain = CurDAG->getCopyToReg(Chain, dl, EZH::R1, SrcHi, InGlue);
      InGlue = Chain.getValue(1);
    } else {
      Chain = CurDAG->getCopyToReg(Chain, dl, EZH::R0, Src, InGlue);
      InGlue = Chain.getValue(1);
    }

    SmallVector<SDValue, 8> Ops;
    Ops.push_back(Callee);
    Ops.push_back(CurDAG->getRegister(EZH::R0, MVT::i32));
    if (VT == MVT::i64) {
      Ops.push_back(CurDAG->getRegister(EZH::R1, MVT::i32));
    }
    Ops.push_back(Chain);
    Ops.push_back(InGlue);

    SDVTList NodeTys = CurDAG->getVTList(MVT::Other, MVT::Glue);
    SDNode *CallNode = CurDAG->getMachineNode(EZH::CALLExt, dl, NodeTys, Ops);

    Chain = SDValue(CallNode, 0);
    InGlue = SDValue(CallNode, 1);

    SDValue Result =
        CurDAG->getCopyFromReg(Chain, dl, EZH::R0, MVT::i32, InGlue);

    ReplaceNode(Node, Result.getNode());
    return;
  }

  unsigned Opcode = Node->getOpcode();

  switch (Opcode) {
  case ISD::Constant: {
    auto *C = cast<ConstantSDNode>(Node);
    int64_t Val = C->getSExtValue();
    if (!isInt<11>(Val)) {
      SDLoc DL(Node);
      SDValue CPIdx = CurDAG->getTargetConstantPool(
          ConstantInt::get(Type::getInt32Ty(*CurDAG->getContext()), Val),
          TLI->getPointerTy(CurDAG->getDataLayout()));

      SDValue Ops[] = {CPIdx, CurDAG->getEntryNode()};
      SDNode *ResNode = CurDAG->getMachineNode(EZH::LOAD_CONSTANT, DL, MVT::i32,
                                               MVT::Other, Ops);

      ReplaceUses(SDValue(Node, 0), SDValue(ResNode, 0));
      CurDAG->RemoveDeadNode(Node);
      return;
    }
    break;
  }
  case ISD::FrameIndex:
    selectFrameIndex(Node);
    return;
  case EZHISD::TC_RETURN: {
    // The memory form of a musttail indirect call carries its target as a
    // FrameIndex (see LowerCall): select it manually to TCRETURN_MEM. A
    // pattern would materialize the slot address into a register, which is
    // exactly what this form exists to avoid. Register and direct targets
    // fall through to the patterns.
    auto *FIN = dyn_cast<FrameIndexSDNode>(Node->getOperand(1));
    if (!FIN)
      break;
    SDLoc DL(Node);
    unsigned NumOps = Node->getNumOperands();
    SDValue Glue;
    if (Node->getOperand(NumOps - 1).getValueType() == MVT::Glue)
      Glue = Node->getOperand(--NumOps);
    SmallVector<SDValue, 8> Ops;
    Ops.push_back(CurDAG->getRegister(EZH::PC, MVT::i32));
    Ops.push_back(CurDAG->getTargetFrameIndex(FIN->getIndex(),
                                              TLI->getPointerTy(
                                                  CurDAG->getDataLayout())));
    Ops.push_back(CurDAG->getTargetConstant(0, DL, MVT::i32));
    Ops.push_back(CurDAG->getTargetConstant(EZHCC::ICC_EU, DL, MVT::i32));
    for (unsigned i = 2; i < NumOps; ++i)
      Ops.push_back(Node->getOperand(i)); // argument-register uses
    Ops.push_back(Node->getOperand(0));   // chain
    if (Glue)
      Ops.push_back(Glue);
    MachineSDNode *Res =
        CurDAG->getMachineNode(EZH::TCRETURN_MEM, DL, MVT::Other, Ops);
    ReplaceNode(Node, Res);
    return;
  }
  case ISD::LOAD:
  case ISD::STORE:
    if (tryIndexedLoadStore(Node))
      return;
    break;
  default:
    break;
  }

  SelectCode(Node);
}

// Match a stack-object address -- a FrameIndex, optionally plus a constant
// offset -- as a (base, offset) memory operand pair, so loads and stores of
// stack objects need no separate address materialization.
// eliminateFrameIndex range-checks the final SP-relative offset and
// scavenges when it does not fit, as it already does for spill slots.
bool EZHDAGToDAGISel::SelectFrameAddr(SDValue Addr, SDValue &Base,
                                      SDValue &Offset) {
  if (auto *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), Addr.getValueType());
    Offset = CurDAG->getTargetConstant(0, SDLoc(Addr), MVT::i32);
    return true;
  }
  if (CurDAG->isBaseWithConstantOffset(Addr)) {
    if (auto *FIN = dyn_cast<FrameIndexSDNode>(Addr.getOperand(0))) {
      int64_t Off = cast<ConstantSDNode>(Addr.getOperand(1))->getSExtValue();
      if (isInt<16>(Off)) {
        Base =
            CurDAG->getTargetFrameIndex(FIN->getIndex(), Addr.getValueType());
        Offset = CurDAG->getTargetConstant(Off, SDLoc(Addr), MVT::i32);
        return true;
      }
    }
  }
  return false;
}

void EZHDAGToDAGISel::selectFrameIndex(SDNode *Node) {
  SDLoc DL(Node);
  SDValue Imm = CurDAG->getTargetConstant(0, DL, MVT::i32);
  int FI = cast<FrameIndexSDNode>(Node)->getIndex();
  EVT VT = Node->getValueType(0);
  SDValue TFI = CurDAG->getTargetFrameIndex(FI, VT);
  unsigned Opc = EZH::ADD_IMM;
  SDValue Pred = CurDAG->getTargetConstant(EZHCC::ICC_EU, DL, MVT::i32);
  if (Node->hasOneUse()) {
    CurDAG->SelectNodeTo(Node, Opc, VT, TFI, Imm, Pred);
    return;
  }
  ReplaceNode(Node, CurDAG->getMachineNode(Opc, DL, VT, TFI, Imm, Pred));
}

FunctionPass *llvm::createEZHISelDag(EZHTargetMachine &TM) {
  return new EZHDAGToDAGISelLegacy(TM);
}
