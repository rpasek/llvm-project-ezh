//===-- EZHInstrInfo.td - EZH Instruction defs ---------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Description:
//   TableGen definitions for all EZH instructions (ALU ops, branches,
//   loads/stores, pseudos), SelectionDAG patterns, and operands.
//
// Copied From:
//   Lanai target backend (llvm/lib/Target/Lanai/LanaiInstrInfo.td).
//
// Changes:
//   Replaced Lanai instruction definitions with EZH records and DAG patterns
//   matching fsl_smartdma_prv.h and AN14650; implemented preceding condition
//   code assembly syntax (e.g., goto_ca); enforced PC-relative load limits
//   (+508/-512 range).
//
//===----------------------------------------------------------------------===//

include "EZHInstrFormats.td"

def EZHNullPat {
  list<dag> Value = [];
}

//===----------------------------------------------------------------------===//
// SDNodes and Profiles
//===----------------------------------------------------------------------===//
def SDT_EZHCallSeqStart : SDCallSeqStart<[SDTCisVT<0, i32>, SDTCisVT<1, i32>]>;
def SDT_EZHCallSeqEnd : SDCallSeqEnd<[SDTCisVT<0, i32>, SDTCisVT<1, i32>]>;
def SDT_EZHCall : SDTypeProfile<0, -1, [SDTCisVT<0, i32>]>;
def EZH_call : SDNode<"EZHISD::CALL", SDT_EZHCall, [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue, SDNPVariadic]>;
def EZH_tcret : SDNode<"EZHISD::TC_RETURN", SDT_EZHCall, [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def EZH_adjcallstackdown : SDNode<"ISD::CALLSEQ_START", SDT_EZHCallSeqStart, [SDNPHasChain, SDNPOutGlue]>;
def EZH_adjcallstackup : SDNode<"ISD::CALLSEQ_END", SDT_EZHCallSeqEnd, [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;
def EZH_ret_glue_node : SDNode<"EZHISD::RET_GLUE_INTERNAL", SDTNone, [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def EZHcmp : SDNode<"EZHISD::CMP", SDTypeProfile<0, 2, [SDTCisSameAs<0, 1>]>, [SDNPOutGlue]>;
def EZHbrcc : SDNode<"EZHISD::BR_CC", SDTypeProfile<0, 2, [SDTCisVT<0, OtherVT>, SDTCisVT<1, i32>]>, [SDNPHasChain, SDNPInGlue]>;
def EZHselectcc : SDNode<"EZHISD::SELECT_CC", SDTypeProfile<1, 3, [SDTCisSameAs<0, 1>, SDTCisSameAs<1, 2>, SDTCisVT<3, i32>]>, [SDNPInGlue]>;
def EZHbtog : SDNode<"EZHISD::BTOG", SDTypeProfile<1, 2, [SDTCisSameAs<0, 1>, SDTCisVT<2, i32>]>>;
def EZHbrjt : SDNode<"EZHISD::BR_JT", SDTypeProfile<0, 3, [SDTCisVT<0, i32>, SDTCisVT<1, i32>, SDTCisVT<2, i32>]>, [SDNPHasChain]>;
def EZHsjlj_setjmp : SDNode<"EZHISD::EH_SJLJ_SETJMP", SDTypeProfile<1, 1, [SDTCisVT<0, i32>, SDTCisVT<1, i32>]>, [SDNPHasChain, SDNPOptInGlue]>;
def EZHsjlj_longjmp : SDNode<"EZHISD::EH_SJLJ_LONGJMP", SDTypeProfile<0, 1, [SDTCisVT<0, i32>]>, [SDNPHasChain, SDNPOptInGlue]>;
def EZHsjlj_setup_dispatch : SDNode<"EZHISD::EH_SJLJ_SETUP_DISPATCH", SDTypeProfile<0, 0, []>, [SDNPHasChain, SDNPOptInGlue]>;

// EZH Peripheral Memory Access Profiles
def SDT_EZHPerRead  : SDTypeProfile<1, 1, [SDTCisVT<0, i32>, SDTCisVT<1, i32>]>;
def SDT_EZHPerWrite : SDTypeProfile<0, 2, [SDTCisVT<0, i32>, SDTCisVT<1, i32>]>;

def EZHper_read  : SDNode<"EZHISD::PER_READ", SDT_EZHPerRead,
                          [SDNPHasChain, SDNPMayLoad, SDNPMemOperand]>;
def EZHper_write : SDNode<"EZHISD::PER_WRITE", SDT_EZHPerWrite,
                           [SDNPHasChain, SDNPMayStore, SDNPMemOperand]>;

//===----------------------------------------------------------------------===//
// Operands
//===----------------------------------------------------------------------===//
def calltarget : Operand<i32>, ImmLeaf<i32, [{ return (Imm & 3) == 0; }]> {
  let PrintMethod = "printBranchTarget";
  let EncoderMethod = "getCallTargetOpValue";
  let DecoderMethod = "decodeSimm30";
}

def brtarget : Operand<OtherVT> {
  let PrintMethod = "printBranchTarget";
  let EncoderMethod = "getBranchTargetOpValue";
  let DecoderMethod = "decodeSimm21";
}

// Symbol target in a goto's 21-bit address field (tail calls): same
// encoding and fixup as brtarget, but i32-typed so it can match
// tglobaladdr/texternalsym in patterns.
def tcalltarget : Operand<i32> {
  let PrintMethod = "printBranchTarget";
  let EncoderMethod = "getBranchTargetOpValue";
  let DecoderMethod = "decodeSimm21";
}

def imm20 : Operand<i32>, ImmLeaf<i32, [{ return isUInt<20>(Imm) && (Imm & 3) == 0; }]> {
  let PrintMethod = "printPerAddrOperand";
  let EncoderMethod = "getPerAddrOpValue";
}

def shifted_imms11 : Operand<i32>, ImmLeaf<i32, [{ return isInt<11>(Imm); }]> {
  let PrintMethod = "printShiftedImmOperand";
  let DecoderMethod = "decodeSimm11";
}

def imms11 : Operand<i32>, ImmLeaf<i32, [{ return isInt<11>(Imm); }]> {
  let PrintMethod = "printImmOperand";
  let DecoderMethod = "decodeSimm11";
}

def imms12 : Operand<i32>, ImmLeaf<i32, [{ return isInt<12>(Imm); }]> {
  let PrintMethod = "printImmOperand";
  let DecoderMethod = "decodeSimm12";
}

def imms8_word : PatLeaf<(imm), [{ return isInt<8>(N->getSExtValue() >> 2) && ((N->getZExtValue() & 3) == 0); }]>;

def imms8_word_offset : Operand<i32>, ImmLeaf<i32, [{ return isInt<8>(Imm >> 2) && ((Imm & 3) == 0); }]> {
  let PrintMethod = "printWordOffset";
  let EncoderMethod = "getWordOffsetOpValue";
  let DecoderMethod = "decodeWordOffset";
}

def imms8 : Operand<i32>, ImmLeaf<i32, [{ return isInt<8>(Imm); }]> {
  let PrintMethod = "printImmOperand";
  let DecoderMethod = "decodeSimm8";
}

def imm5 : Operand<i32>, ImmLeaf<i32, [{ return isUInt<5>(Imm); }]> {
  let PrintMethod = "printImmOperand";
  // Range-check the 5-bit field at encode time (bit positions / shift amounts).
  let EncoderMethod = "getImm5OpValue";
}

// Unsigned 8-bit mask fields (the acc_vectored_hold dispatch-enable mask and
// the modify_gpo_byte AND/OR/XOR masks). Distinct from imms8: the field is a
// bit mask, so it decodes and prints unsigned (0xFF is 255, not -1) and the
// encoder range-checks [0,255].
def uimm8 : Operand<i32>, ImmLeaf<i32, [{ return isUInt<8>(Imm); }]> {
  let PrintMethod = "printImmOperand";
  let EncoderMethod = "getUimm8OpValue";
}

def imm16 : Operand<i32>, ImmLeaf<i32, [{ return isUInt<16>(Imm); }]> {
  let PrintMethod = "printImmOperand";
  let DecoderMethod = "decodeSimm16";
}

def imm24 : Operand<i32>, ImmLeaf<i32, [{ return isUInt<24>(Imm); }]> {
  let PrintMethod = "printImmOperand";
}

def imm1 : Operand<i32>, ImmLeaf<i32, [{ return isInt<1>(Imm) || isUInt<1>(Imm); }]> {
  let PrintMethod = "printImmOperand";
}

//===----------------------------------------------------------------------===//
// Condition Codes & Flags
//===----------------------------------------------------------------------===//
class EZHCondCode<bits<4> val, string suffix> {
  bits<4> Value = val;
  string Suffix = suffix;
}

def Cond_eu : EZHCondCode<COND_EU, "">;     // Always / True (EU flag)
def Cond_ze : EZHCondCode<COND_ZE, "_ze">;  // Zero (ZE flag)
def Cond_nz : EZHCondCode<COND_NZ, "_nz">;  // Not Zero (NZ flag)
def Cond_po : EZHCondCode<COND_PO, "_po">;  // Positive (PO flag)
def Cond_ne : EZHCondCode<COND_NE, "_ne">;  // Negative (NE flag)
def Cond_az : EZHCondCode<COND_AZ, "_az">;  // Above Zero (AZ flag)
def Cond_zb : EZHCondCode<COND_ZB, "_zb">;  // Zero or Below (ZB flag)
def Cond_ca : EZHCondCode<COND_CA, "_ca">;  // Carry (CA flag)
def Cond_nc : EZHCondCode<COND_NC, "_nc">;  // No Carry (NC flag)
def Cond_cz : EZHCondCode<COND_CZ, "_cz">;  // Carry or Zero (CZ flag)
def Cond_spo: EZHCondCode<COND_SPO, "_spo">; // Shift-Only-when-Positive / Unconditionally Scheduled branch delay slot (SPO/UNS)
def Cond_sne: EZHCondCode<COND_SNE, "_sne">; // Shift-Only-when-Negative / Scheduled branch delay slot when Not Zero (SNE/NZS)
def Cond_nbs: EZHCondCode<COND_NBS, "_nbs">; // Not Boolean-expression set (NBS flag)
def Cond_nex: EZHCondCode<COND_NEX, "_nex">; // External flag is not set (NEX flag)
def Cond_bs : EZHCondCode<COND_BS, "_bs">;  // Boolean-expression set (BS flag)
def Cond_ex : EZHCondCode<COND_EX, "_ex">;  // External flag is set (EX flag)

defvar CondCodes = [Cond_eu, Cond_ze, Cond_nz, Cond_po, Cond_ne, Cond_az, Cond_zb, Cond_ca, Cond_nc, Cond_cz, Cond_spo, Cond_sne, Cond_nbs, Cond_nex, Cond_bs, Cond_ex];

  class SetFlag<bits<1> val, string suffix> {
  bits<1> Value = val;
  string Suffix = suffix;
}

def Flag_none : SetFlag<0, "">;
def Flag_s : SetFlag<1, "s">;

defvar SetFlags = [Flag_none, Flag_s];

//===----------------------------------------------------------------------===//
// Helper Classes & Multiclasses
//===----------------------------------------------------------------------===//
class EZHPseudo<dag outs, dag ins, string asmstr, list<dag> pattern>
    : Instruction {
  let Namespace = "EZH";
  dag OutOperandList = outs;
  dag InOperandList = ins;
  let AsmString = asmstr;
  let Pattern = pattern;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
}

// Helpers for Power of 2 matching
def Log2 : SDNodeXForm<imm, [{ return CurDAG->getTargetConstant(Log2_32(N->getZExtValue()), SDLoc(N), MVT::i32); }]>;
def Log2Inv : SDNodeXForm<imm, [{ return CurDAG->getTargetConstant(Log2_32(~N->getZExtValue()), SDLoc(N), MVT::i32); }]>;
def power_of_2 : PatLeaf<(imm), [{ return isPowerOf2_32(N->getZExtValue()); }], Log2>;
def power_of_2_inv : PatLeaf<(imm), [{ return isPowerOf2_32(~N->getZExtValue()); }], Log2Inv>;

class ShiftInfo<bits<3> val, string name, string asm_name, SDPatternOperator node, bit flip> {
  bits<3> Val = val;
  string Name = name;
  string AsmName = asm_name;
  SDPatternOperator Node = node;
  bit Flip = flip;
}

class EZHShiftAluOpInfo<bits<3> op, string name, string upper_name, SDPatternOperator node, bit rev = 0, bit has_pat = 1> {
  bits<3> AluOp = op;
  string Name = name;
  string UpperName = upper_name;
  SDPatternOperator Node = node;
  bit Rev = rev;
  bit HasPat = has_pat;
}

def EZHShiftAluOp_AND : EZHShiftAluOpInfo<FLIP_AND, "and", "AND", and>;
def EZHShiftAluOp_OR  : EZHShiftAluOpInfo<FLIP_OR,  "or",  "OR",  or>;
def EZHShiftAluOp_XOR : EZHShiftAluOpInfo<FLIP_XOR, "xor", "XOR", xor>;
def EZHShiftAluOp_ADD : EZHShiftAluOpInfo<FLIP_ADD, "add", "ADD", add>;
def EZHShiftAluOp_SUB : EZHShiftAluOpInfo<FLIP_SUB, "sub", "SUB", sub, 1>;
def EZHShiftAluOp_ADC : EZHShiftAluOpInfo<FLIP_ADC, "adc", "ADC", null_frag, 0, 0>;
def EZHShiftAluOp_SBC : EZHShiftAluOpInfo<FLIP_SBC, "sbc", "SBC", null_frag, 0, 0>;

defvar EZHShiftAluOpsList = [
  EZHShiftAluOp_AND, EZHShiftAluOp_OR, EZHShiftAluOp_XOR,
  EZHShiftAluOp_ADD, EZHShiftAluOp_SUB, EZHShiftAluOp_ADC, EZHShiftAluOp_SBC
];

multiclass MemOps<bits<5> major_op, bit is_store, bit is_word, bit is_signed,
                      string opc_name, Operand offset_type,
                      list<dag> pattern,
                      string asm_ops_normal, string asm_ops_pre, string asm_ops_post> {

  defvar outs_normal = !if(is_store, (outs), (outs GPR:$Rd));
  defvar ins_normal  = !if(is_store, (ins GPR:$Rd, GPR:$Rn, offset_type:$Offset),
                                     (ins GPR:$Rn, offset_type:$Offset));
  defvar ins_normal_pred = !con(ins_normal, (ins pred:$Cond));

  defvar outs_wb     = !if(is_store, (outs GPR:$Rn_wb), (outs GPR:$Rd, GPR:$Rn_wb));

  // Normal
  def ""
    : EZHInstMem<major_op, is_store, is_word, is_signed, 0, 0,
                 outs_normal, ins_normal_pred,
                 opc_name#"$Cond "#asm_ops_normal,
                 pattern> {}

  // Pre-indexed. Selected manually in EZHISelDAGToDAG (empty pattern), so
  // tablegen cannot infer mayLoad/mayStore as it does for the plain form above;
  // set it explicitly or these are marked with conservative unmodeled side
  // effects and machine AA / scheduling treat them as non-memory. (The
  // base-register write-back is modeled by the $Rn_wb def.)
  def _PRE
    : EZHInstMem<major_op, is_store, is_word, is_signed, 0, 1,
                 outs_wb, ins_normal_pred,
                 opc_name#"_pre$Cond "#asm_ops_pre,
                 []> {
    let Constraints = "$Rn = $Rn_wb";
    let mayLoad = !if(is_store, 0, 1);
    let mayStore = is_store;
  }

  // Post-indexed
  def _POST
    : EZHInstMem<major_op, is_store, is_word, is_signed, 1, 1,
                 outs_wb, ins_normal_pred,
                 opc_name#"_post$Cond "#asm_ops_post,
                 []> {
    let Constraints = "$Rn = $Rn_wb";
    let mayLoad = !if(is_store, 0, 1);
    let mayStore = is_store;
  }
}



multiclass ALUShiftedOps<bits<5> op, string opc_str, string sf_suffix> {
  defvar ins_shifted_pred = (ins GPR:$Rs1, GPR:$Rs2, imm5:$ShAmt, pred:$Cond);

  // Normal and Negated Shifts
  foreach sh = EZHShiftTypesList in {
    foreach inv = [0, 1] in {
      defvar inv_suffix = !if(inv, "n", "");
      defvar name_suffix = !if(!empty(sf_suffix), "", "_" # sf_suffix);

      def !if(inv, "N_", "_") # sh.UpperName # name_suffix
        : EZHInstALUShifted<op, sh.Val, inv, (outs GPR:$Rd), ins_shifted_pred,
                            opc_str # inv_suffix # "_" # sh.Name # sf_suffix # "$Cond $Rd, $Rs1, $Rs2, $ShAmt", []>;
    }
  }

  // Fast Shifts (ShAmt = 0, shift_type = 4)
  foreach inv = [0, 1] in {
    defvar inv_suffix = !if(inv, "n", "");
    defvar name_suffix = !if(!empty(sf_suffix), "", "_" # sf_suffix);

    def !if(inv, "N_F", "_F") # name_suffix
      : EZHInstALUShifted<op, 4, inv, (outs GPR:$Rd), (ins GPR:$Rs1, GPR:$Rs2, pred:$Cond),
                          opc_str # inv_suffix # "_f" # sf_suffix # "$Cond $Rd, $Rs1, $Rs2", []> {
      let ShAmt = 0;
    }
  }
}

multiclass RegShiftBaseOps<bits<2> shift_type, string shift_name, SDPatternOperator shift_node,
                           string sf_suffix, dag ins_basic> {
  defvar ins_basic_pred = !con(ins_basic, (ins pred:$Cond));

  foreach is_inv = [0, 1] in {
    defvar inv_suffix = !if(is_inv, "N", "");
    defvar name_inv_suffix = !if(is_inv, "n", "");

    def !if(is_inv, "N", "") # !if(!empty(sf_suffix), "", "_" # sf_suffix)
      : EZHInstRegShiftALU<0, shift_type, 0, is_inv, (outs GPR:$Rd), ins_basic_pred,
                            "r"#shift_name#name_inv_suffix#sf_suffix#"$Cond $Rd, $Rs2, $Rshift",
                            !if(is_inv,
                                [(set i32:$Rd, (not (shift_node i32:$Rs2, i32:$Rshift)))],
                                [(set i32:$Rd, (shift_node i32:$Rs2, i32:$Rshift))])> {
      let Rs1 = 0;
    }
  }
}

multiclass RegShiftALUOps<bits<2> shift_type, string shift_name, SDPatternOperator shift_node,
                          string sf_suffix, bits<1> is_post, dag ins_alu> {
  defvar ins_alu_pred = !con(ins_alu, (ins pred:$Cond));

  foreach alu = EZHShiftAluOpsList in {
    foreach is_inv = [0, 1] in {
      defvar inv_suffix = !if(is_inv, "N", "");
      defvar name_inv_suffix = !if(is_inv, "n", "");

      defvar shifted = (shift_node i32:$Rs2, i32:$Rshift);

      defvar alu_op_pre = !if(alu.Rev, (alu.Node shifted, i32:$Rs1), (alu.Node i32:$Rs1, shifted));
      defvar pre_pat = !if(is_inv, (not alu_op_pre), alu_op_pre);

      defvar alu_op_post = !if(alu.Rev, (alu.Node i32:$Rs2, i32:$Rs1), (alu.Node i32:$Rs1, i32:$Rs2));
      defvar post_pat = (shift_node !if(is_inv, (not alu_op_post), alu_op_post), i32:$Rshift);

      defvar selected_pat = !if(is_post, post_pat, pre_pat);
      defvar pat = !if(alu.HasPat, [(set i32:$Rd, selected_pat)], []<dag>);

      def "_" # alu.UpperName # inv_suffix # !if(!empty(sf_suffix), "", "_" # sf_suffix)
        : EZHInstRegShiftALU<alu.AluOp, shift_type, is_post, is_inv, (outs GPR:$Rd), ins_alu_pred,
                             !if(is_post,
                                 alu.Name # name_inv_suffix # "_r" # shift_name # sf_suffix # "$Cond $Rd, $Rs1, $Rs2, $Rshift",
                                 "r" # shift_name # "_" # alu.Name # name_inv_suffix # sf_suffix # "$Cond $Rd, $Rs1, $Rs2, $Rshift"),
                             pat>;
    }
  }
}

multiclass RegShiftOps<bits<2> shift_type, string shift_name, SDPatternOperator shift_node,
                        string sf_suffix,
                        dag ins_basic, dag ins_alu> {
  defvar ins_basic_pred = !con(ins_basic, (ins pred:$Cond));
  defvar ins_alu_pred = !con(ins_alu, (ins pred:$Cond));

  // Basic shift: RLSL, RLSR, RASR, RROR
  def !if(!empty(sf_suffix), "", "_" # sf_suffix)
    : EZHInstRegShiftALU<0, shift_type, 0, 0, (outs GPR:$Rd), ins_basic_pred,
                         "r"#shift_name#sf_suffix#"$Cond $Rd, $Rs2, $Rshift",
                         [(set i32:$Rd, (shift_node i32:$Rs2, i32:$Rshift))]> {
    let Rs1 = 0;
  }

  // Shift then ALU: RLSL_AND, RLSL_OR ...
  foreach alu = EZHShiftAluOpsList in {
    def "_" # alu.UpperName # !if(!empty(sf_suffix), "", "_" # sf_suffix)
      : EZHInstRegShiftALU<alu.AluOp, shift_type, 0, 0, (outs GPR:$Rd), ins_alu_pred,
                           "r"#shift_name#"_"#alu.Name#sf_suffix#"$Cond $Rd, $Rs1, $Rs2, $Rshift",
                           !if(alu.HasPat,
                               [(set i32:$Rd, !if(alu.Rev,
                                                  (alu.Node (shift_node i32:$Rs2, i32:$Rshift), i32:$Rs1),
                                                  (alu.Node i32:$Rs1, (shift_node i32:$Rs2, i32:$Rshift))))],
                               [])>;
  }
}

multiclass ShiftOps<bits<2> basic_shift_type, bits<2> reg_shift_type, string shift_name, SDPatternOperator shift_node,
                    string sf_suffix,
                    dag ins_shift, dag ins_shift_alu,
                    dag ins_reg_shift, dag ins_reg_shift_alu> {
  defvar ins_shift_pred = !con(ins_shift, (ins pred:$Cond));
  defvar ins_shift_alu_pred = !con(ins_shift_alu, (ins pred:$Cond));

  // Basic shift: LSL, LSR, ASR, ROR (no "i" suffix!)
  def "" # !if(!empty(sf_suffix), "", "_" # sf_suffix)
    : EZHInstShift<basic_shift_type, (outs GPR:$Rd), ins_shift_pred,
                   shift_name#sf_suffix#"$Cond $Rd, $Rs, $ShAmt",
                   [(set i32:$Rd, (shift_node i32:$Rs, (i32 imm:$ShAmt)))]>;

  // Shift-ALU: LSL_AND, LSL_OR ... generated dynamically
  foreach alu = EZHShiftAluOpsList in {
    def "_" # alu.UpperName # !if(!empty(sf_suffix), "", "_" # sf_suffix)
      : EZHInstShiftALU<alu.AluOp, reg_shift_type, (outs GPR:$Rd), ins_shift_alu_pred,
                        shift_name#"_"#alu.Name#sf_suffix#"$Cond $Rd, $Rs1, $Rs2, $ShAmt",
                        !if(alu.HasPat,
                            [(set i32:$Rd, !if(alu.Rev,
                                               (alu.Node (shift_node i32:$Rs2, (i32 imm:$ShAmt)), i32:$Rs1),
                                               (alu.Node i32:$Rs1, (shift_node i32:$Rs2, (i32 imm:$ShAmt)))))],
                            [])>;
  }

  // Register Shifts (Pseudonymous defm with R prefix!)
  defm R#NAME : RegShiftBaseOps<reg_shift_type, shift_name, shift_node, sf_suffix,
                               ins_reg_shift>;
  defm R#NAME#_PRE : RegShiftALUOps<reg_shift_type, shift_name, shift_node, sf_suffix, 0,
                                   ins_reg_shift_alu>;
  defm R#NAME#_POST : RegShiftALUOps<reg_shift_type, shift_name, shift_node, sf_suffix, 1,
                                    ins_reg_shift_alu>;
}

multiclass FlipShiftOps<bit is_fbit, string opc_name, string sf_suffix, dag ins_flip> {
  defvar ins_flip_pred = !con(ins_flip, (ins pred:$Cond));

  def "_LSR" # !if(!empty(sf_suffix), "", "_" # sf_suffix) : EZHInstFlipALU<0, 0, is_fbit, (outs GPR:$Rd), ins_flip_pred,
                                                 opc_name#"_lsr"#sf_suffix#"$Cond $Rd, $Rs2, $ShAmt", []> {
    let Rs1 = 0;
  }
  def "_ASR" # !if(!empty(sf_suffix), "", "_" # sf_suffix) : EZHInstFlipALU<0, 1, is_fbit, (outs GPR:$Rd), ins_flip_pred,
                                                 opc_name#"_asr"#sf_suffix#"$Cond $Rd, $Rs2, $ShAmt", []> {
    let Rs1 = 0;
  }
}

multiclass FlipALUOps<bits<3> op, string opc_str, string sf_suffix> {
  defvar ins_flip = (ins GPR:$Rs1, GPR:$Rs2, imm5:$ShAmt, pred:$Cond);

  def "_FBIT_LSR" # !if(!empty(sf_suffix), "", "_" # sf_suffix) : EZHInstFlipALU<op, 0, 1, (outs GPR:$Rd), ins_flip, "fbit_lsr_"#opc_str#sf_suffix#"$Cond $Rd, $Rs1, $Rs2, $ShAmt", []>;
  def "_FBIT_ASR" # !if(!empty(sf_suffix), "", "_" # sf_suffix) : EZHInstFlipALU<op, 1, 1, (outs GPR:$Rd), ins_flip, "fbit_asr_"#opc_str#sf_suffix#"$Cond $Rd, $Rs1, $Rs2, $ShAmt", []>;
  def "_FEND_LSR" # !if(!empty(sf_suffix), "", "_" # sf_suffix) : EZHInstFlipALU<op, 0, 0, (outs GPR:$Rd), ins_flip, "fend_lsr_"#opc_str#sf_suffix#"$Cond $Rd, $Rs1, $Rs2, $ShAmt", []>;
  def "_FEND_ASR" # !if(!empty(sf_suffix), "", "_" # sf_suffix) : EZHInstFlipALU<op, 1, 0, (outs GPR:$Rd), ins_flip, "fend_asr_"#opc_str#sf_suffix#"$Cond $Rd, $Rs1, $Rs2, $ShAmt", []>;
}

multiclass ALUOps<bits<5> op, bits<3> flip_op, string opc_str,
                   string sf_suffix,
                   dag ins_rr, dag ins_ri,
                   RegisterClass rc_ri = GPR,
                   SDPatternOperator op_node = null_frag,
                   SDPatternOperator op_node_e = null_frag> {
  defvar ins_rr_pred = !con(ins_rr, (ins pred:$Cond));
  defvar ins_ri_pred = !con(ins_ri, (ins pred:$Cond));

  // Register-Register (no suffix)
  def "" # !if(!empty(sf_suffix), "", "_" # sf_suffix) : EZHInstALU<op, 0, (outs GPR:$Rd), ins_rr_pred,
    opc_str#sf_suffix#"$Cond $Rd, $Rs1, $Rs2",
    !if(!and(!empty(sf_suffix), !ne(op_node, null_frag)),
        [(set GPR:$Rd, (op_node GPR:$Rs1, GPR:$Rs2))],
        !if(!and(!empty(sf_suffix), !ne(op_node_e, null_frag)),
            [(set GPR:$Rd, (op_node_e GPR:$Rs1, GPR:$Rs2))],
            []))>;

  // Register-Register Negated (N)
  def "N" # !if(!empty(sf_suffix), "", "_" # sf_suffix) : EZHInstALU<op, 1, (outs GPR:$Rd), ins_rr_pred,
    opc_str#"n"#sf_suffix#"$Cond $Rd, $Rs1, $Rs2",
    !if(!and(!empty(sf_suffix), !ne(op_node, null_frag)),
        [(set GPR:$Rd, (not (op_node GPR:$Rs1, GPR:$Rs2)))],
        [])>;

  // Register-Immediate (_IMM)
  def "_IMM" # !if(!empty(sf_suffix), "", "_" # sf_suffix) : EZHInstALUI12<op, 0, (outs rc_ri:$Rd), ins_ri_pred,
    opc_str#"_imm"#sf_suffix#"$Cond $Rd, $Rs1, $Imm",
    !if(!and(!empty(sf_suffix), !ne(op_node, null_frag)),
        [(set GPR:$Rd, (op_node GPR:$Rs1, (i32 imm:$Imm)))],
        !if(!and(!empty(sf_suffix), !ne(op_node_e, null_frag)),
            [(set GPR:$Rd, (op_node_e GPR:$Rs1, (i32 imm:$Imm)))],
            []))>;

  // Register-Immediate Negated (N_IMM)
  def "N_IMM" # !if(!empty(sf_suffix), "", "_" # sf_suffix) : EZHInstALUI12<op, 1, (outs GPR:$Rd), ins_ri_pred,
    opc_str#"n_imm"#sf_suffix#"$Cond $Rd, $Rs1, $Imm",
    !if(!and(!empty(sf_suffix), !ne(op_node, null_frag)),
        [(set GPR:$Rd, (not (op_node GPR:$Rs1, (i32 imm:$Imm))))],
        [])>;

  // Flip ALU Operations (Anonymous defm)
  defm "" : FlipALUOps<flip_op, opc_str, sf_suffix>;
}

multiclass UnaryALUOps<bits<5> op, string opc_str, string sf_suffix, bit is_mvn = 0,
                       SDPatternOperator op_node = null_frag> {
  defvar ins_GPR_pred = (ins GPR:$Rs1, pred:$Cond);

  def "" # !if(!empty(sf_suffix), "", "_" # sf_suffix)
    : EZHInstALU<op, 0, (outs GPR:$Rd), ins_GPR_pred,
                 opc_str#sf_suffix#"$Cond $Rd, $Rs1",
                 !if(!and(!empty(sf_suffix), !ne(op_node, null_frag)),
                     [(set GPR:$Rd, (op_node GPR:$Rs1))],
                     [])> {
    let Rs2 = 0;
    let Inst{31} = is_mvn;
  }
}

multiclass LoadImmOps<string opc_str, string sf_suffix, bit is_immn = 0> {
  defvar ins_imm_pred = (ins imms11:$Imm, pred:$Cond);

  def "" # !if(!empty(sf_suffix), "", "_" # sf_suffix)
    : EZHInstALUI<OP_MOV, 0, (outs GPR:$Rd), ins_imm_pred,
                  opc_str#sf_suffix#"$Cond $Rd, $Imm",
                  !if(!empty(sf_suffix),
                      !if(is_immn,
                          [(set GPR:$Rd, (not (i32 imm:$Imm)))],
                          [(set GPR:$Rd, (i32 imm:$Imm))]),
                      [])> {
    let Rs1 = 0;
    let Inst{31} = is_immn;
  }
}

multiclass LoadSimmOps<string opc_str, string sf_suffix, bit invert_val> {
  defvar ins_simm_pred = (ins shifted_imms11:$Imm, imm5:$ShAmt, pred:$Cond);

  let AsmMatchConverter = "cvtShiftedImm" in {
    def "" # !if(!empty(sf_suffix), "", "_" # sf_suffix)
      : EZHInstALUIShifted<OP_MOV, invert_val, (outs GPR:$Rd), ins_simm_pred,
                           opc_str#sf_suffix#"$Cond $Rd, $Imm",
                           []>;
  }
}

multiclass BitOps<bits<2> op_type, bit s_bit, string opc_str, string sf_suffix> {
  defvar ins_reg_pred = (ins GPR:$Rs, GPR:$Rbit, pred:$Cond);
  defvar ins_imm_pred = (ins GPR:$Rs, imm5:$Bit5, pred:$Cond);

  def "" # !if(!empty(sf_suffix), "", "_" # sf_suffix)
    : EZHInstBitReg<op_type, (outs GPR:$Rd), ins_reg_pred,
                    opc_str#sf_suffix#"$Cond $Rd, $Rs, $Rbit",
                    []> {
    let S_bit = s_bit;
  }

  def "_IMM" # !if(!empty(sf_suffix), "", "_" # sf_suffix)
    : EZHInstBit<op_type, (outs GPR:$Rd), ins_imm_pred,
                 opc_str#"_imm"#sf_suffix#"$Cond $Rd, $Rs, $Bit5",
                 []> {
    let S_bit = s_bit;
  }
}

multiclass AndOrOps<string sf_suffix> {
  // Rs3 (the rotate amount, bits[27:24]) accepts only R0-R7 on silicon --
  // NXP-confirmed, same restriction family as the 0x19 rshift and the 0x1D
  // index register.
  defvar ins_andor_pred = (ins GPR:$Rs1, GPR:$Rs2, GPR3Bit:$Rs3, pred:$Cond);

  def "" # !if(!empty(sf_suffix), "", "_" # sf_suffix)
    : EZHInstAndOr<(outs GPR:$Rd), ins_andor_pred,
                   "andor"#sf_suffix#"$Cond $Rd, $Rs1, $Rs2, $Rs3",
                   []>;
}

//===----------------------------------------------------------------------===//
// Pseudo Instructions
//===----------------------------------------------------------------------===//
let mayLoad = 1 in {
  def LOAD_CONSTANT : EZHPseudo<(outs GPR:$Rd), (ins i32imm:$imm), "", []>;
  def LOAD_CONSTANT_COND : EZHPseudo<(outs GPR:$Rd), (ins i32imm:$imm, pred:$Cond), "", []>;
}
let isNotDuplicable = 1, isBarrier = 1, hasSideEffects = 1 in
def CONSTPOOL_ENTRY : EZHPseudo<(outs), (ins unknown:$sym, unknown:$val), "$sym:\n\t.long $val", []>;

let isCall = 1, Defs = [R0, R1, R2, R3, RA], isCodeGenOnly = 1 in {
  def CALL_INDIRECT : EZHInstGotoReg<1, (outs), (ins GPRNoFPBP:$raddr), "goto_regl $raddr", [(EZH_call i32:$raddr)]> {
    let isPredicable = 0;
    let Cond = 0;
  }
}

let Defs = [RA], isTerminator = 1, isBarrier = 1, isBranch = 1, isIndirectBranch = 1 in
def PseudoBR_JT : EZHPseudo<(outs), (ins GPR:$table, GPR:$index, i32imm:$jti), "# PseudoBR_JT $table, $index, $jti", [(EZHbrjt i32:$table, i32:$index, tjumptable:$jti)]>;

let isBranch = 1, isTerminator = 1, isBarrier = 1, isIndirectBranch = 1, mayLoad = 1 in
def BRIND_LDR : EZHPseudo<(outs), (ins GPR:$Rn, imms8_word_offset:$Offset, pred:$Cond),
                          "# BRIND_LDR $Rn, $Offset", []>;

let usesCustomInserter = 1, hasSideEffects = 1 in {
  def PseudoCMP : EZHPseudo<(outs), (ins GPR:$lhs, GPR:$rhs), "", [(EZHcmp i32:$lhs, i32:$rhs)]>;
  def PseudoCMPi : EZHPseudo<(outs), (ins GPR:$lhs, imms12:$rhs), "", [(EZHcmp i32:$lhs, imms12:$rhs)]>;
}

let usesCustomInserter = 1, isTerminator = 1, isBranch = 1 in {
  def PseudoBR_CC : EZHPseudo<(outs), (ins brtarget:$dest, i32imm:$cc), "", [(EZHbrcc bb:$dest, timm:$cc)]>;
}

let usesCustomInserter = 1 in {
  def PseudoSELECT_CC : EZHPseudo<(outs GPR:$dst), (ins GPR:$truev, GPR:$falsev, i32imm:$cc), "", [(set i32:$dst, (EZHselectcc i32:$truev, i32:$falsev, timm:$cc))]>;
}

let isPseudo = 1, hasSideEffects = 1 in {
  def ADJCALLSTACKDOWN : EZHPseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
    "",
    [(EZH_adjcallstackdown timm:$amt1, timm:$amt2)]>;
  def ADJCALLSTACKUP : EZHPseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
    "",
    [(EZH_adjcallstackup timm:$amt1, timm:$amt2)]>;

  let usesCustomInserter = 1 in {
    let Defs = [R0, R1, R2, R3, R4, R5, R6, R7, RA], isBarrier = 1 in {
      def EH_SjLj_SetJmp : EZHPseudo<(outs GPR:$dst), (ins GPR:$buf), "",
                          [(set i32:$dst, (EZHsjlj_setjmp i32:$buf))]>;
    }
    let isTerminator = 1, isBarrier = 1 in {
      def EH_SjLj_LongJmp : EZHPseudo<(outs), (ins GPR:$buf), "",
                            [(EZHsjlj_longjmp i32:$buf)]>;
    }
    let isBarrier = 1, hasSideEffects = 1 in {
      def EH_SjLj_Setup_Dispatch : EZHPseudo<(outs), (ins), "",
                            [(EZHsjlj_setup_dispatch)]>;
    }
  }
}

//===----------------------------------------------------------------------===//
// Real Instructions
//===----------------------------------------------------------------------===//

// HOLD family
def HOLD : EZHCondInst<(outs), (ins pred:$Cond), "hold$Cond", [(int_ezh_hold)]> {
  let Opcode = OP_HOLD;
  let Inst{13-10} = REG_PC; // PC
  let Inst{14} = 0;
  let Inst{15} = 1;
  let Inst{31-16} = 0;
  let Inst{9} = 0; // Force S bit to 0
}
// Vectored holds. Two distinct register roles (see EZHInstrFormats.td): $Rd
// receives the hardware-computed vector (table + 4 + 4*slice) on a
// dispatching wake, $table is the jump-table base. The plain (non-ACC) forms
// encode an empty dispatch-enable mask and never dispatch -- they wake and
// resume inline; only the acc_* forms (with a nonzero $vectors mask) vector.
// __builtin_ezh_acc_vectored_hold exposes the base ACC form: it returns the
// vector address, which the caller dispatches through (e.g. mov pc). All
// silicon-validated; the _lv bit semantics remain unconfirmed, so those
// variants stay assembler-only.
//
// The base (non-_nra) forms hardware-write RA with the resume address on a
// dispatching wake -- NVIC-style linkage so a dispatched handler can return.
// That is what the _nra ("no RA") variants opt out of. Model it: without
// Defs = [RA] the allocator assumes RA survives the hold, and any function
// that returns after using the builtin jumps back into its own loop instead
// (found on silicon: the vectored two-source demo never exited main).
let Defs = [RA] in {
def VECTORED_HOLD : EZHInstVectoredHold<0, 0, (outs GPR:$Rd), (ins GPR:$table, pred:$Cond), "vectored_hold$Cond $Rd, $table", []>;
def VECTORED_HOLD_LV : EZHInstVectoredHold<0, 1, (outs GPR:$Rd), (ins GPR:$table, pred:$Cond), "vectored_hold_lv$Cond $Rd, $table", []>;
}
def VECTORED_HOLD_NRA : EZHInstVectoredHold<1, 0, (outs GPR:$Rd), (ins GPR:$table, pred:$Cond), "vectored_hold_nra$Cond $Rd, $table", []>;
def VECTORED_HOLD_LV_NRA : EZHInstVectoredHold<1, 1, (outs GPR:$Rd), (ins GPR:$table, pred:$Cond), "vectored_hold_lv_nra$Cond $Rd, $table", []>;
// @earlyclobber: the hardware writes $Rd while still needing the value in
// $table (silicon: the same-register form does not deliver a usable vector),
// so the allocator must not coalesce them.
let Defs = [RA] in {
let Constraints = "@earlyclobber $Rd" in
def ACC_VECTORED_HOLD : EZHInstAccVectoredHold<0, 0, (outs GPR:$Rd), (ins GPR:$table, uimm8:$vectors, pred:$Cond), "acc_vectored_hold$Cond $Rd, $table, $vectors", [(set i32:$Rd, (int_ezh_acc_vectored_hold GPR:$table, timm:$vectors))]>;
def ACC_VECTORED_HOLD_LV : EZHInstAccVectoredHold<0, 1, (outs GPR:$Rd), (ins GPR:$table, uimm8:$vectors, pred:$Cond), "acc_vectored_hold_lv$Cond $Rd, $table, $vectors", []>;
}
def ACC_VECTORED_HOLD_NRA : EZHInstAccVectoredHold<1, 0, (outs GPR:$Rd), (ins GPR:$table, uimm8:$vectors, pred:$Cond), "acc_vectored_hold_nra$Cond $Rd, $table, $vectors", []>;
def ACC_VECTORED_HOLD_LV_NRA : EZHInstAccVectoredHold<1, 1, (outs GPR:$Rd), (ins GPR:$table, uimm8:$vectors, pred:$Cond), "acc_vectored_hold_lv_nra$Cond $Rd, $table, $vectors", []>;

// Tight Loop. The pattern makes the OP_LOOP hardware loop reachable from C as
// __builtin_ezh_tight_loop(rend, rcount) -- an expert, event-paced primitive
// (see IntrinsicsEZH.td for the body contract). Silicon-validated: Rcount+1
// iterations of the single straight-line body ending at the address in Rend
// when each iteration holds on an armed bitslice event; a free-running loop
// executes the body zero times.
def TIGHT_LOOP : EZHInstTightLoop<(outs), (ins GPR:$Rend, GPR:$Rcount, pred:$Cond), "tight_loop$Cond $Rend, $Rcount", [(int_ezh_tight_loop GPR:$Rend, GPR:$Rcount)]>;

// Reg Offset Load/Store. address = Rn + Rm (Rm is a byte offset), validated on
// silicon. The patterns fold a runtime-indexed access (a[i]) into one
// instruction; they sit below the immediate-offset LDR/STR above, so a constant
// in-range offset still prefers ldr/str. The pred:$Cond operand is auto-filled
// with the "always" predicate for these unconditional patterns.
// The index register Rm (bits[27:24]) accepts only R0-R7 on silicon --
// NXP-confirmed operand restriction (same field carries the same restriction
// on the 0x19 register-shift rshift and the 0x16 andor ror operand). GPR3Bit
// makes the assembler reject specials instead of silently mis-encoding them;
// codegen was already safe (the specials are reserved from allocation).
def LDR_REG : EZHInstMemReg<OP_MEM_REG, 0, 1, 0, (outs GPR:$Rd), (ins GPR:$Rn, GPR3Bit:$Rm, pred:$Cond), "ldr_reg$Cond $Rd, $Rn, $Rm",
                            [(set i32:$Rd, (load (add i32:$Rn, i32:$Rm)))]>;
def LDR_REGB : EZHInstMemReg<OP_MEM_REG, 0, 0, 0, (outs GPR:$Rd), (ins GPR:$Rn, GPR3Bit:$Rm, pred:$Cond), "ldr_regb$Cond $Rd, $Rn, $Rm",
                             [(set i32:$Rd, (zextloadi8 (add i32:$Rn, i32:$Rm)))]>;
def LDR_REGBS : EZHInstMemReg<OP_MEM_REG, 0, 0, 1, (outs GPR:$Rd), (ins GPR:$Rn, GPR3Bit:$Rm, pred:$Cond), "ldr_regbs$Cond $Rd, $Rn, $Rm",
                              [(set i32:$Rd, (sextloadi8 (add i32:$Rn, i32:$Rm)))]>;
def STR_REG : EZHInstMemReg<OP_MEM_REG, 1, 1, 0, (outs), (ins GPR:$Rd, GPR:$Rn, GPR3Bit:$Rm, pred:$Cond), "str_reg$Cond $Rn, $Rd, $Rm",
                            [(store i32:$Rd, (add i32:$Rn, i32:$Rm))]>;
def STR_REGB : EZHInstMemReg<OP_MEM_REG, 1, 0, 0, (outs), (ins GPR:$Rd, GPR:$Rn, GPR3Bit:$Rm, pred:$Cond), "str_regb$Cond $Rn, $Rd, $Rm",
                             [(truncstorei8 i32:$Rd, (add i32:$Rn, i32:$Rm))]>;

// Push / Pop (Upward Stack)
def PUSH : EZHInstMem<OP_STR, 1, 1, 0, 1, 1, (outs), (ins GPR:$Rd, pred:$Cond), "push$Cond $Rd", []> {
  let Rn = REG_SP;
  let Offset = 1;
}
def POP : EZHInstMem<OP_LDR, 0, 1, 0, 0, 1, (outs GPR:$Rd), (ins pred:$Cond), "pop$Cond $Rd", []> {
  let Rn = REG_SP;
  let Offset = 0xFF;
}
def PUSHB : EZHInstMem<OP_STR, 1, 0, 0, 1, 1, (outs), (ins GPR:$Rd, pred:$Cond), "pushb$Cond $Rd", []> {
  let Rn = REG_SP;
  let Offset = 1;
}
def POPB : EZHInstMem<OP_LDR, 0, 0, 0, 0, 1, (outs GPR:$Rd), (ins pred:$Cond), "popb$Cond $Rd", []> {
  let Rn = REG_SP;
  let Offset = 0xFF;
}
def POPBS : EZHInstMem<OP_LDR, 0, 0, 1, 0, 1, (outs GPR:$Rd), (ins pred:$Cond), "popbs$Cond $Rd", []> {
  let Rn = REG_SP;
  let Offset = 0xFF;
}

// Push / Pop Down (Downward Stack)
def PUSHD : EZHInstMem<OP_STR, 1, 1, 0, 0, 1, (outs), (ins GPR:$Rd, pred:$Cond), "pushd$Cond $Rd", []> {
  let Rn = REG_SP;
  let Offset = 0xFF;
}
def POPD : EZHInstMem<OP_LDR, 0, 1, 0, 1, 1, (outs GPR:$Rd), (ins pred:$Cond), "popd$Cond $Rd", []> {
  let Rn = REG_SP;
  let Offset = 1;
}
def PUSHDB : EZHInstMem<OP_STR, 1, 0, 0, 0, 1, (outs), (ins GPR:$Rd, pred:$Cond), "pushdb$Cond $Rd", []> {
  let Rn = REG_SP;
  let Offset = 0xFF;
}
def POPDB : EZHInstMem<OP_LDR, 0, 0, 0, 1, 1, (outs GPR:$Rd), (ins pred:$Cond), "popdb$Cond $Rd", []> {
  let Rn = REG_SP;
  let Offset = 1;
}
def POPDBS : EZHInstMem<OP_LDR, 0, 0, 1, 1, 1, (outs GPR:$Rd), (ins pred:$Cond), "popdbs$Cond $Rd", []> {
  let Rn = REG_SP;
  let Offset = 1;
}

// Peripheral Read/Write
def PER_READ : EZHInstPerRead<OP_PER_READ, (outs GPR:$Rd), (ins imm20:$addr, pred:$Cond), "per_read$Cond $Rd, $addr", []>;
def PER_WRITE : EZHInstPerWrite<OP_PER_WRITE, (outs), (ins GPR:$Rs, imm20:$addr, pred:$Cond), "per_write$Cond $Rs, $addr", []>;

//===----------------------------------------------------------------------===//
// Predicated ALU & Shift Instructions
//===----------------------------------------------------------------------===//

defvar ins_ALU_rr = (ins GPR:$Rs1, GPR:$Rs2);
defvar ins_ALU_ri = (ins GPR:$Rs1, imms12:$Imm);
defvar ins_ALU_reg_shift = (ins GPR:$Rs1, GPR:$Rs2, GPR3Bit:$Rshift);

defvar ins_Shift_s = (ins GPR:$Rs, imm5:$ShAmt);
defvar ins_Shift_sa = (ins GPR:$Rs1, GPR:$Rs2, imm5:$ShAmt);
defvar ins_Shift_rs = (ins GPR:$Rs2, GPR3Bit:$Rshift);
defvar ins_Shift_rsa = (ins GPR:$Rs1, GPR:$Rs2, GPR3Bit:$Rshift);
defvar ins_Shift_f = (ins GPR:$Rs2, imm5:$ShAmt); // Flip shifts expect Rs2!

foreach sf = SetFlags in {
  defvar sf_suffix = sf.Suffix;
  let S = sf.Value in {
    // ALU
    defm ADD : ALUOps<ALU_ADD, FLIP_ADD, "add", sf_suffix, ins_ALU_rr, ins_ALU_ri, GPR, add, null_frag>;
    defm SUB : ALUOps<ALU_SUB, FLIP_SUB, "sub", sf_suffix, ins_ALU_rr, ins_ALU_ri, GPR, sub, null_frag>;

    // Post-Shifted ALU
    defm ADD : ALUShiftedOps<ALU_ADD, "add", sf_suffix>;
    defm SUB : ALUShiftedOps<ALU_SUB, "sub", sf_suffix>;
    defm ADC : ALUShiftedOps<ALU_ADC, "adc", sf_suffix>;
    defm SBC : ALUShiftedOps<ALU_SBC, "sbc", sf_suffix>;
    defm OR  : ALUShiftedOps<ALU_OR,  "or",  sf_suffix>;
    defm AND : ALUShiftedOps<ALU_AND, "and", sf_suffix>;
    defm XOR : ALUShiftedOps<ALU_XOR, "xor", sf_suffix>;
    defm ADC : ALUOps<ALU_ADC, FLIP_ADC, "adc", sf_suffix, ins_ALU_rr, ins_ALU_ri, GPR, null_frag, adde>;
    defm SBC : ALUOps<ALU_SBC, FLIP_SBC, "sbc", sf_suffix, ins_ALU_rr, ins_ALU_ri, GPR, null_frag, sube>;
    defm OR  : ALUOps<ALU_OR,  FLIP_OR,  "or",  sf_suffix, ins_ALU_rr, ins_ALU_ri, GPR, or, null_frag>;
    defm AND : ALUOps<ALU_AND, FLIP_AND, "and", sf_suffix, ins_ALU_rr, ins_ALU_ri, GPR, and, null_frag>;
    defm XOR : ALUOps<ALU_XOR, FLIP_XOR, "xor", sf_suffix, ins_ALU_rr, ins_ALU_ri, GPR, xor, null_frag>;

    // Shifts
    defm LSL : ShiftOps<SHIFT3_LSL, SHIFT2_LSL, "lsl", shl, sf_suffix, ins_Shift_s, ins_Shift_sa, ins_Shift_rs, ins_Shift_rsa>;
    defm LSR : ShiftOps<SHIFT3_LSR, SHIFT2_LSR, "lsr", srl, sf_suffix, ins_Shift_s, ins_Shift_sa, ins_Shift_rs, ins_Shift_rsa>;
    defm ASR : ShiftOps<SHIFT3_ASR, SHIFT2_ASR, "asr", sra, sf_suffix, ins_Shift_s, ins_Shift_sa, ins_Shift_rs, ins_Shift_rsa>;
    defm ROR : ShiftOps<SHIFT3_ROR, SHIFT2_ROR, "ror", rotr, sf_suffix, ins_Shift_s, ins_Shift_sa, ins_Shift_rs, ins_Shift_rsa>;

    // Flip Shifts
    defm FBIT : FlipShiftOps<1, "fbit", sf_suffix, ins_Shift_f>;
    defm FEND : FlipShiftOps<0, "fend", sf_suffix, ins_Shift_f>;

    // Unary ALU
    defm MOV : UnaryALUOps<OP_MOV, "mov", sf_suffix, 0>;
    defm MVN : UnaryALUOps<OP_MOV, "mvn", sf_suffix, 1, not>;

    // Load Imm
    defm LOAD_IMM : LoadImmOps<"load_imm", sf_suffix, 0>;
    defm LOAD_IMMN : LoadImmOps<"load_immn", sf_suffix, 1>;

    // Load Shifted Imm
    defm LOAD_SIMM : LoadSimmOps<"load_simm", sf_suffix, 0>;
    defm LOAD_SIMMN : LoadSimmOps<"load_simmn", sf_suffix, 1>;

    // Bit Ops
    defm BSET : BitOps<2, 1, "bset", sf_suffix>;
    defm BCLR : BitOps<1, 0, "bclr", sf_suffix>;
    defm BTST : BitOps<1, 1, "btst", sf_suffix>;
    defm BTOG : BitOps<3, 1, "btog", sf_suffix>;

    // ANDOR
    defm ANDOR : AndOrOps<sf_suffix>;
  }
}

//===----------------------------------------------------------------------===//
// Predicated Memory Instructions
//===----------------------------------------------------------------------===//

defm LDR : MemOps<OP_LDR, 0, 1, 0, "ldr",   imms8_word_offset,
  [(set i32:$Rd, (load (add i32:$Rn, imms8_word:$Offset)))],
  "$Rd, $Rn, $Offset", "$Rd, $Rn, $Offset", "$Rn, $Rd, $Offset">;

defm STR : MemOps<OP_STR, 1, 1, 0, "str",   imms8_word_offset,
  [(store i32:$Rd, (add i32:$Rn, imms8_word:$Offset))],
  "$Rn, $Rd, $Offset", "$Rn, $Rd, $Offset", "$Rn, $Rd, $Offset">;

defm LDRB : MemOps<OP_LDR, 0, 0, 0, "ldrb",  imms8,
  [(set i32:$Rd, (zextloadi8 (add i32:$Rn, imms8:$Offset)))],
  "$Rd, $Rn, $Offset", "$Rd, $Rn, $Offset", "$Rd, $Rn, $Offset">;

defm LDRBS : MemOps<OP_LDR, 0, 0, 1, "ldrbs", imms8,
  [(set i32:$Rd, (sextloadi8 (add i32:$Rn, imms8:$Offset)))],
  "$Rd, $Rn, $Offset", "$Rd, $Rn, $Offset", "$Rd, $Rn, $Offset">;

defm STRB : MemOps<OP_STR, 1, 0, 0, "strb",  imms8,
  [(truncstorei8 i32:$Rd, (add i32:$Rn, imms8:$Offset))],
  "$Rn, $Rd, $Offset", "$Rn, $Rd, $Offset", "$Rn, $Rd, $Offset">;

//===----------------------------------------------------------------------===//
// Predicated Branch Instructions
//===----------------------------------------------------------------------===//

let isBranch = 1, isTerminator = 1, isBarrier = 1 in {
  def GOTO : EZHInstGoto<0, (outs), (ins brtarget:$addr, pred:$Cond), "goto$Cond $addr", [(br bb:$addr)]>;
  def GOTOL : EZHInstGoto<1, (outs), (ins brtarget:$addr, pred:$Cond), "gotol$Cond $addr", []> { let Defs = [RA]; }
  def GOTO_REG : EZHInstGotoReg<0, (outs), (ins GPR:$raddr, pred:$Cond), "goto_reg$Cond $raddr", [(brind i32:$raddr)]>;
  def GOTO_REGL : EZHInstGotoReg<1, (outs), (ins GPR:$raddr, pred:$Cond), "goto_regl$Cond $raddr", []> { let Defs = [RA]; }
}

// Unconditional instructions
def NOP : EZHInstSingle<OP_NOP, (outs), (ins), "nop", [(int_ezh_nop)]> { let Inst{31-5} = 0; }
def INT_TRIGGER : EZHInstSingle<OP_INT_TRIGGER, (outs), (ins imm24:$Imm), "int_trigger $Imm", [(int_ezh_int_trigger timm:$Imm)]> { bits<24> Imm; let Inst{31-8} = Imm; let Inst{7-5} = 0; }
let Defs = [GPO], Uses = [GPO] in
def MODIFY_GPO_BYTE : EZHInstModifyGPOByte<(outs), (ins uimm8:$AndMask, uimm8:$OrMask, uimm8:$XorMask),
                        "modify_gpo_byte $AndMask, $OrMask, $XorMask",
                        [(int_ezh_modify_gpo_byte timm:$AndMask, timm:$OrMask, timm:$XorMask)]>;

// GPIO intrinsic instructions. These are the first defs that select LLVM
// intrinsics; the matching __builtin_ezh_* are wired automatically through the
// ClangBuiltin association (no manual CodeGen). GPI and GPD are reserved from
// allocation (EZHRegisterInfo.cpp), so they are hardwired here. EZHInstALU /
// EZHInstBit already carry hasSideEffects = 1, which matches the intrinsics'
// IntrHasSideEffects -- the GPIO state is invisible to the optimizer otherwise.

// These hardwire a reserved special register into an otherwise-generic mov /
// bit-op, so they exist only for instruction selection: the assembler and
// disassembler round-trip them through the generic form with the register name
// (mov $Rd, gpi == this READ_GPI). isCodeGenOnly keeps them out of the matcher
// and decoder, where they would otherwise collide with the generic mov/bset_imm.
// Side-effect ordering comes from the IntrHasSideEffects on their intrinsic
// patterns (TableGen infers hasSideEffects = 1 from the pattern).
let isCodeGenOnly = 1 in {

// Read the GPI input register (special reg 14): mov $Rd, gpi.
let Uses = [GPI] in
def READ_GPI : EZHInstALU<OP_MOV, 0, (outs GPR:$Rd), (ins pred:$Cond),
                          "mov$Cond $Rd, gpi", [(set i32:$Rd, (int_ezh_read_gpi))]> {
  let Rs1 = REG_GPI;
  let Rs2 = 0;
  let Inst{31} = 0; // matches mov (is_mvn = 0)
}

// Read the GPD drive-enable register (special reg 9): mov $Rd, gpd.
let Uses = [GPD] in
def READ_GPD : EZHInstALU<OP_MOV, 0, (outs GPR:$Rd), (ins pred:$Cond),
                          "mov$Cond $Rd, gpd", [(set i32:$Rd, (int_ezh_read_gpd))]> {
  let Rs1 = REG_GPD;
  let Rs2 = 0;
  let Inst{31} = 0;
}

// Open-drain line control via the GPD drive-enable register: set bit N to pull
// the pin low, clear bit N to release it. GPD is hardwired as both source and
// destination of a single bit-op. The bit position is a 0..31 immediate.
let Defs = [GPD], Uses = [GPD] in {
def GPD_DRIVE_LOW : EZHInstBit<2, (outs), (ins imm5:$Bit5, pred:$Cond),
                               "bset_imm$Cond gpd, gpd, $Bit5",
                               [(int_ezh_gpd_drive_low timm:$Bit5)]> {
  let Rd = REG_GPD;
  let Rs = REG_GPD;
  let S_bit = 1; // matches bset_imm
}
def GPD_RELEASE : EZHInstBit<1, (outs), (ins imm5:$Bit5, pred:$Cond),
                             "bclr_imm$Cond gpd, gpd, $Bit5",
                             [(int_ezh_gpd_release timm:$Bit5)]> {
  let Rd = REG_GPD;
  let Rs = REG_GPD;
  let S_bit = 0; // matches bclr_imm
}
}

// Full-width GPO output register (special reg 8): mov to/from. The runtime,
// all-32-bit counterpart of MODIFY_GPO_BYTE (whose masks are constant and
// limited to GPO[7:0]); this is the parallel-bus output path.
let Uses = [GPO] in
def READ_GPO : EZHInstALU<OP_MOV, 0, (outs GPR:$Rd), (ins pred:$Cond),
                          "mov$Cond $Rd, gpo", [(set i32:$Rd, (int_ezh_read_gpo))]> {
  let Rs1 = REG_GPO;
  let Rs2 = 0;
  let Inst{31} = 0;
}
let Defs = [GPO] in
def WRITE_GPO : EZHInstALU<OP_MOV, 0, (outs), (ins GPR:$Rs1, pred:$Cond),
                           "mov$Cond gpo, $Rs1", [(int_ezh_write_gpo i32:$Rs1)]> {
  let Rd = REG_GPO;
  let Rs2 = 0;
  let Inst{31} = 0;
}

// Full-width GPD drive-enable write (special reg 9): atomic multi-line
// open-drain update with a runtime mask; complements the constant-bit
// GPD_DRIVE_LOW/GPD_RELEASE pair and the existing READ_GPD.
let Defs = [GPD] in
def WRITE_GPD : EZHInstALU<OP_MOV, 0, (outs), (ins GPR:$Rs1, pred:$Cond),
                           "mov$Cond gpd, $Rs1", [(int_ezh_write_gpd i32:$Rs1)]> {
  let Rd = REG_GPD;
  let Rs2 = 0;
  let Inst{31} = 0;
}

// CFM single-bit set/clear: the 1-instruction slice arm/disarm. Same
// hardwired bit-op shape as the GPD pair above. Silicon note: a CFM bit-op
// does NOT clear the sticky bitslice flags -- only a full mov-to-CFM write
// (WRITE_CFM) does.
let Defs = [CFM], Uses = [CFM] in {
def CFM_BSET : EZHInstBit<2, (outs), (ins imm5:$Bit5, pred:$Cond),
                          "bset_imm$Cond cfm, cfm, $Bit5",
                          [(int_ezh_cfm_bset timm:$Bit5)]> {
  let Rd = REG_CFM;
  let Rs = REG_CFM;
  let S_bit = 1; // matches bset_imm
}
def CFM_BCLR : EZHInstBit<1, (outs), (ins imm5:$Bit5, pred:$Cond),
                          "bclr_imm$Cond cfm, cfm, $Bit5",
                          [(int_ezh_cfm_bclr timm:$Bit5)]> {
  let Rd = REG_CFM;
  let Rs = REG_CFM;
  let S_bit = 0; // matches bclr_imm
}
}

// Bitslice config/status registers CFM (special reg 11) and CFS (special reg
// 10): readable and writable as a mov to/from the reserved special register.
let Uses = [CFM] in
def READ_CFM : EZHInstALU<OP_MOV, 0, (outs GPR:$Rd), (ins pred:$Cond),
                          "mov$Cond $Rd, cfm", [(set i32:$Rd, (int_ezh_read_cfm))]> {
  let Rs1 = REG_CFM;
  let Rs2 = 0;
  let Inst{31} = 0;
}
let Defs = [CFM] in
def WRITE_CFM : EZHInstALU<OP_MOV, 0, (outs), (ins GPR:$Rs1, pred:$Cond),
                           "mov$Cond cfm, $Rs1", [(int_ezh_write_cfm i32:$Rs1)]> {
  let Rd = REG_CFM;
  let Rs2 = 0;
  let Inst{31} = 0;
}
let Uses = [CFS] in
def READ_CFS : EZHInstALU<OP_MOV, 0, (outs GPR:$Rd), (ins pred:$Cond),
                          "mov$Cond $Rd, cfs", [(set i32:$Rd, (int_ezh_read_cfs))]> {
  let Rs1 = REG_CFS;
  let Rs2 = 0;
  let Inst{31} = 0;
}
let Defs = [CFS] in
def WRITE_CFS : EZHInstALU<OP_MOV, 0, (outs), (ins GPR:$Rs1, pred:$Cond),
                           "mov$Cond cfs, $Rs1", [(int_ezh_write_cfs i32:$Rs1)]> {
  let Rd = REG_CFS;
  let Rs2 = 0;
  let Inst{31} = 0;
}

} // isCodeGenOnly = 1 (hardwired special-register mov/bit-op forms)

def HEART_RYTHM_IMM : EZHInstSingle<OP_NOP, (outs), (ins imm16:$Imm), "heart_rythm_imm $Imm", [(int_ezh_heart_rythm_imm timm:$Imm)]> {
  bits<16> Imm;
  let Inst{31-16} = Imm;
  let Inst{15-6} = 0;
  let Inst{5} = 1;
}

def HEART_RYTHM : EZHInstSingle<OP_NOP, (outs), (ins GPR:$Rs), "heart_rythm $Rs", [(int_ezh_heart_rythm i32:$Rs)]> {
  bits<4> Rs;
  let Inst{31-18} = 0;
  let Inst{17-14} = Rs;
  let Inst{13-10} = 0;
  let Inst{9} = 1;
  let Inst{8-6} = 0;
  let Inst{5} = 1;
}

def WAIT_FOR_BEAT : EZHInstSingle<OP_NOP, (outs), (ins), "wait_for_beat", [(int_ezh_wait_for_beat)]> {
  let Inst{31-7} = 0;
  let Inst{6} = 1;
  let Inst{5} = 1;
  let Inst{4-0} = OP_NOP;
}

def SYNCH_ALL_TO_BEAT : EZHInstSingle<OP_NOP, (outs), (ins imm1:$Imm), "synch_all_to_beat $Imm", [(int_ezh_synch_all_to_beat timm:$Imm)]> {
  bits<1> Imm;
  let Inst{31} = Imm;
  let Inst{30-7} = 0;
  let Inst{6} = 1;
  let Inst{5} = 0;
  let Inst{4-0} = OP_NOP;
}

// Call instructions
// Tail call: a plain goto (no RA write) in return position, so the callee
// returns directly to this function's caller. Same 21-bit encoding and
// fixup as any direct goto. EZHBitSliceInjection places the tail call's
// interrupt poll BEFORE the epilogue rather than before the goto itself:
// by the goto, the epilogue has restored the live return address into RA,
// which the injected gotol_bs would clobber.
let isCall = 1, isReturn = 1, isTerminator = 1, isBarrier = 1,
    isCodeGenOnly = 1, Uses = [SP] in {
  def TCRETURN : EZHInstGoto<0, (outs), (ins tcalltarget:$addr, pred:$Cond),
                             "goto$Cond $addr", [(EZH_tcret tglobaladdr:$addr)]>;
  def TCRETURNExt : EZHInstGoto<0, (outs), (ins tcalltarget:$addr, pred:$Cond),
                                "goto$Cond $addr", [(EZH_tcret texternalsym:$addr)]>;
  // Indirect form (function-pointer tail calls, e.g. interpreter dispatch).
  // The address register is restricted to GPRTC because the epilogue's pops
  // execute before the goto_reg.
  def TCRETURN_REG : EZHInstGotoReg<0, (outs), (ins GPRTC:$raddr, pred:$Cond),
                                    "goto_reg$Cond $raddr",
                                    [(EZH_tcret i32:$raddr)]>;
}

let isCall = 1, Defs = [R0, R1, R2, R3, RA] in {
  def CALL : EZHInstCall<OP_GOSUB, (outs), (ins calltarget:$addr), "gosub $addr", [(EZH_call tglobaladdr:$addr)]>;
  let isCodeGenOnly = 1 in
  def CALLExt : EZHInstCall<OP_GOSUB, (outs), (ins calltarget:$addr), "gosub $addr", [(EZH_call texternalsym:$addr)]>;
}

// Return
let isReturn = 1, isTerminator = 1, isBarrier = 1 in {
  def RET : EZHInstGotoReg<0, (outs), (ins variable_ops), "goto ra", [(EZH_ret_glue_node)]> {
    let raddr = REG_RA;
    let isPredicable = 0;
    let Cond = 0;
  }
}

//===----------------------------------------------------------------------===//
// Instruction Selection Patterns
//===----------------------------------------------------------------------===//
def : Pat<(EZHbtog i32:$Rs, (i32 imm:$Bit)), (BTOG_IMM GPR:$Rs, imm:$Bit, 0)>;

// Memory Patterns (Words and Bytes)
// 16-bit access is handled by the Legalizer's Expand mechanism into bytes.
// Memory Patterns (Words and Bytes)
// 16-bit access is handled by the Legalizer's Expand mechanism into bytes.
def : Pat<(i32 (load GPR:$Rn)), (LDR GPR:$Rn, 0, 0)>;
def : Pat<(store i32:$Rd, GPR:$Rn), (STR GPR:$Rd, GPR:$Rn, 0, 0)>;

def : Pat<(i32 (zextloadi8 GPR:$Rn)), (LDRB GPR:$Rn, 0, 0)>;
def : Pat<(i32 (sextloadi8 GPR:$Rn)), (LDRBS GPR:$Rn, 0, 0)>;
def : Pat<(i32 (extloadi8 GPR:$Rn)), (LDRB GPR:$Rn, 0, 0)>;
def : Pat<(truncstorei8 GPR:$Rd, GPR:$Rn), (STRB GPR:$Rd, GPR:$Rn, 0, 0)>;

def : Pat<(i32 (extloadi1 GPR:$Rn)), (LDRB GPR:$Rn, 0, 0)>;
def : Pat<(i32 (zextloadi1 GPR:$Rn)), (LDRB GPR:$Rn, 0, 0)>;

//===----------------------------------------------------------------------===//
// Instruction Selection Patterns (ISel lowering)
//===----------------------------------------------------------------------===//

// Selection patterns for peripheral read/write (target-specific nodes)
def : Pat<(EZHper_read timm:$addr),
          (PER_READ timm:$addr, 0)>;

def : Pat<(EZHper_write GPR:$Rs, timm:$addr),
          (PER_WRITE GPR:$Rs, timm:$addr, 0)>;

// Variable shift patterns
def : Pat<(shl GPR:$Rs2, GPR3Bit:$Rshift), (RLSL GPR:$Rs2, GPR3Bit:$Rshift, 0)>;
def : Pat<(srl GPR:$Rs2, GPR3Bit:$Rshift), (RLSR GPR:$Rs2, GPR3Bit:$Rshift, 0)>;
def : Pat<(sra GPR:$Rs2, GPR3Bit:$Rshift), (RASR GPR:$Rs2, GPR3Bit:$Rshift, 0)>;

def : Pat<(not (shl GPR:$Rs2, GPR3Bit:$Rshift)), (RLSLN GPR:$Rs2, GPR3Bit:$Rshift, 0)>;
def : Pat<(not (srl GPR:$Rs2, GPR3Bit:$Rshift)), (RLSRN GPR:$Rs2, GPR3Bit:$Rshift, 0)>;
def : Pat<(not (sra GPR:$Rs2, GPR3Bit:$Rshift)), (RASRN GPR:$Rs2, GPR3Bit:$Rshift, 0)>;


// Negate-Result ALU
def : Pat<(not (add i32:$Rs1, i32:$Rs2)), (ADDN GPR:$Rs1, GPR:$Rs2, 0)>;
def : Pat<(not (sub i32:$Rs1, i32:$Rs2)), (SUBN GPR:$Rs1, GPR:$Rs2, 0)>;
def : Pat<(not (and i32:$Rs1, i32:$Rs2)), (ANDN GPR:$Rs1, GPR:$Rs2, 0)>;
def : Pat<(not (or i32:$Rs1, i32:$Rs2)), (ORN GPR:$Rs1, GPR:$Rs2, 0)>;
def : Pat<(not (xor i32:$Rs1, i32:$Rs2)), (XORN GPR:$Rs1, GPR:$Rs2, 0)>;

// Combined AND-then-OR
def : Pat<(or (and i32:$Rs1, i32:$Rs2), i32:$Rs3), (ANDOR GPR:$Rs1, GPR:$Rs2, GPR:$Rs3, 0)>;

// Dynamic Bit Manipulation
def : Pat<(or i32:$Rs, (shl 1, i32:$Rbit)), (BSET GPR:$Rs, GPR:$Rbit, 0)>;
def : Pat<(and i32:$Rs, (not (shl 1, i32:$Rbit))), (BCLR GPR:$Rs, GPR:$Rbit, 0)>;
def : Pat<(xor i32:$Rs, (shl 1, i32:$Rbit)), (BTOG GPR:$Rs, GPR:$Rbit, 0)>;
def : Pat<(and i32:$Rs, (shl 1, i32:$Rbit)), (BTST GPR:$Rs, GPR:$Rbit, 0)>;

// Immediate Bit Manipulation (Power of 2 Matching)
def : Pat<(or i32:$Rs, power_of_2:$Bit), (BSET_IMM GPR:$Rs, power_of_2:$Bit, 0)>;
def : Pat<(and i32:$Rs, power_of_2_inv:$Bit), (BCLR_IMM GPR:$Rs, power_of_2_inv:$Bit, 0)>;
def : Pat<(xor i32:$Rs, power_of_2:$Bit), (BTOG_IMM GPR:$Rs, power_of_2:$Bit, 0)>;

def : Pat<(bswap i32:$Rs2), (FEND_LSR GPR:$Rs2, 0, 0)>;
def : Pat<(bitreverse i32:$Rs2), (FBIT_LSR GPR:$Rs2, 0, 0)>;

// Indirect branch folding patterns
def : Pat<(brind (load (add GPR:$Rn, imms8_word:$Offset))),
          (BRIND_LDR GPR:$Rn, imms8_word:$Offset, (i32 0))>;
def : Pat<(brind (load GPR:$Rn)),
          (BRIND_LDR GPR:$Rn, 0, (i32 0))>;
