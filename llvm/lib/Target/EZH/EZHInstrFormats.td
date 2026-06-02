//===-- EZHInstrFormats.td - EZH Instruction Formats ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Description:
//   TableGen base instruction format classes defining binary machine code
//   fields and bit layouts for EZH instructions.
//
// Copied From:
//   Lanai target backend (llvm/lib/Target/Lanai/LanaiInstrFormats.td).
//
// Changes:
//   Replaced Lanai instruction bit-fields with 32-bit EZH instruction encoding
//   layouts and opcode fields matching machine code specifications in
//   fsl_smartdma_prv.h.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// EZH Major and Sub-Opcodes (from AN14650.txt & fsl_smartdma_prv.h)
//===----------------------------------------------------------------------===//
// Major Opcodes (Inst{4-0})
defvar OP_MOV          = 0x00; // Load/Store Immediate, Register Copy
defvar OP_LDR          = 0x01; // Load memory
defvar OP_STR          = 0x02; // Store memory
defvar OP_GOSUB        = 0x03; // Call (gosub / gosubl)
defvar OP_PER_READ     = 0x04; // Peripheral read
defvar OP_PER_WRITE    = 0x05; // Peripheral write
defvar OP_ALU          = 0x06; // Arithmetic Logic Unit (ADD, SUB, etc.)
defvar OP_CALL         = 0x10; // Call (gosub / gosubl with register)
defvar OP_SHIFT        = 0x10; // Shift (RLSL, RLSR, etc.)
defvar OP_SHIFT_ALU    = 0x10; // Shifted ALU (LSL_ADD, etc.)
defvar OP_FLIP         = 0x11; // Flip ALU (endian/bit flip)
defvar OP_NOP          = 0x12; // No Operation
defvar OP_INT_TRIGGER  = 0x14; // Interrupt trigger
defvar OP_GOTO         = 0x15; // Branch (goto)
defvar OP_ANDOR        = 0x16; // AND-then-OR
defvar OP_BIT          = 0x18; // Bit Manipulation (BSET, BCLR, etc.)
defvar OP_REG_SHIFT    = 0x19; // Register-Shifted ALU
defvar OP_LOOP         = 0x1A; // Tight Loop (hardware loop)
defvar OP_HOLD         = 0x1C; // Hold
defvar OP_MEM_REG      = 0x1D; // Register-Offset Memory Access
defvar OP_MODIFY_GPO   = 0x1E; // Modify GPO Byte

// ALU Sub-Opcodes (used as 'op' in EZHInstALU and other ALU classes)
defvar ALU_AND         = 0x0D;
defvar ALU_OR          = 0x0C;
defvar ALU_XOR         = 0x0E;
defvar ALU_ADD         = 0x06;
defvar ALU_SUB         = 0x08;
defvar ALU_ADC         = 0x09;
defvar ALU_SBC         = 0x0A;

// Flip ALU Sub-Opcodes (used as 'alu_op' in EZHInstFlipALU)
defvar FLIP_NONE       = 0x00;
defvar FLIP_AND        = 0x01;
defvar FLIP_OR         = 0x02;
defvar FLIP_XOR        = 0x03;
defvar FLIP_ADD        = 0x04;
defvar FLIP_SUB        = 0x05;
defvar FLIP_ADC        = 0x06;
defvar FLIP_SBC        = 0x07;

// EZH Register Codes (from fsl_smartdma_prv.h)
defvar REG_R0          = 0x00;
defvar REG_R1          = 0x01;
defvar REG_R2          = 0x02;
defvar REG_R3          = 0x03;
defvar REG_R4          = 0x04;
defvar REG_R5          = 0x05;
defvar REG_R6          = 0x06;
defvar REG_R7          = 0x07;
defvar REG_GPO         = 0x08;
defvar REG_GPD         = 0x09;
defvar REG_CFS         = 0x0a;
defvar REG_CFM         = 0x0b;
defvar REG_SP          = 0x0c;
defvar REG_PC          = 0x0d;
defvar REG_GPI         = 0x0e;
defvar REG_RA          = 0x0f;

// EZH Condition Codes (from fsl_smartdma_prv.h)
defvar COND_EU         = 0x0;
defvar COND_ZE         = 0x1;
defvar COND_NZ         = 0x2;
defvar COND_PO         = 0x3;
defvar COND_NE         = 0x4;
defvar COND_AZ         = 0x5;
defvar COND_ZB         = 0x6;
defvar COND_CA         = 0x7;
defvar COND_NC         = 0x8;
defvar COND_CZ         = 0x9;
defvar COND_SPO        = 0xA;
defvar COND_SNE        = 0xB;
defvar COND_NBS        = 0xC;
defvar COND_NEX        = 0xD;
defvar COND_BS         = 0xE;
defvar COND_EX         = 0xF;

// EZH Shift Types (3-bit, used in EZHInstALUShifted)
defvar SHIFT3_LSL      = 0;
defvar SHIFT3_ASR      = 1;
defvar SHIFT3_LSR      = 2;
defvar SHIFT3_ROR      = 3;
defvar SHIFT3_FLSL     = 4;
defvar SHIFT3_FASR     = 5;
defvar SHIFT3_FLSR     = 6;
defvar SHIFT3_FROR     = 7;

// EZH Shift Types (2-bit, used in EZHInstRegShift and EZHInstRegShiftALU)
defvar SHIFT2_LSL      = 0;
defvar SHIFT2_LSR      = 1;
defvar SHIFT2_ASR      = 2;
defvar SHIFT2_ROR      = 3;

class ShiftTypeInfo<bits<3> val, string name, string upper_name> {
  bits<3> Val = val;
  string Name = name;
  string UpperName = upper_name;
}

def EZHShiftType_LSL  : ShiftTypeInfo<SHIFT3_LSL,  "lsl",  "LSL">;
def EZHShiftType_ASR  : ShiftTypeInfo<SHIFT3_ASR,  "asr",  "ASR">;
def EZHShiftType_LSR  : ShiftTypeInfo<SHIFT3_LSR,  "lsr",  "LSR">;
def EZHShiftType_ROR  : ShiftTypeInfo<SHIFT3_ROR,  "ror",  "ROR">;
def EZHShiftType_FLSL : ShiftTypeInfo<SHIFT3_FLSL, "flsl", "FLSL">;
def EZHShiftType_FASR : ShiftTypeInfo<SHIFT3_FASR, "fasr", "FASR">;
def EZHShiftType_FLSR : ShiftTypeInfo<SHIFT3_FLSR, "flsr", "FLSR">;
def EZHShiftType_FROR : ShiftTypeInfo<SHIFT3_FROR, "fror", "FROR">;

defvar EZHShiftTypesList = [
  EZHShiftType_LSL,  EZHShiftType_ASR,  EZHShiftType_LSR,  EZHShiftType_ROR,
  EZHShiftType_FLSL, EZHShiftType_FASR, EZHShiftType_FLSR, EZHShiftType_FROR
];

// EZH Bit Operation Sub-Opcodes (used in EZHInstBit and EZHInstBitReg)
defvar BIT_TST         = 1;
defvar BIT_CLR         = 1;
defvar BIT_SET         = 2;
defvar BIT_TOG         = 3;

class EZHInst<dag outs, dag ins, string asmstr, list<dag> pattern>
    : Instruction {
  field bits<32> Inst;

  let Namespace = "EZH";
  dag OutOperandList = outs;
  dag InOperandList = ins;
  let AsmString = asmstr;
  let Pattern = pattern;

  let Size = 4;

  // Basic fields
  bits<5> Opcode;

  let Inst{4-0} = Opcode;

  let Predicates = [];

  // TSFlags
  bit isWordMem = 0;
  bit isPredicated = 0;
  bit isPredicable = 0;
  let TSFlags{0} = isWordMem;
  let TSFlags{1} = isPredicated;
  let TSFlags{2} = isPredicable;
}

// Base class for all conditional instructions
class EZHCondInst<dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHInst<outs, ins, asmstr, pattern> {
  bits<4> Cond;
  let Inst{8-5} = Cond;
  let isPredicable = 1;
}

// Base class for all conditional ALU instructions (which have an S bit)
class EZHALUInst<dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHCondInst<outs, ins, asmstr, pattern> {
  bits<1> S = 0; // Set flags
  let Inst{9} = S;
}

// Format for ALU instructions (Register-Register)
class EZHInstALU<bits<5> op, bits<1> invert, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHALUInst<outs, ins, asmstr, pattern> {
  let Opcode = op;
  bits<4> Rd;
  bits<4> Rs1;
  bits<4> Rs2;
  let hasSideEffects = 1;

  let Inst{13-10} = Rd;
  let Inst{17-14} = Rs1;
  let Inst{18}    = 0; // Register mode
  let Inst{19}    = invert;
  let Inst{23-20} = Rs2;
}

// Format for ALU instructions with shifted register
class EZHInstALUShifted<bits<5> op, bits<3> sh_type, bits<1> invert, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHALUInst<outs, ins, asmstr, pattern> {
  let Opcode = op;
  bits<4> Rd;
  bits<4> Rs1;
  bits<4> Rs2;
  bits<5> ShAmt;
  let hasSideEffects = 1;

  let Inst{13-10} = Rd;
  let Inst{17-14} = Rs1;
  let Inst{23-20} = Rs2;
  let Inst{28-24} = ShAmt;
  let Inst{31-29} = sh_type;
  let Inst{19}    = invert;
  let Inst{18}    = 0;
}

// Format for ALU instructions (Register-Immediate)
class EZHInstALUI<bits<5> op, bits<1> invert, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHALUInst<outs, ins, asmstr, pattern> {
  let Opcode = op;
  bits<4> Rd;
  bits<4> Rs1;
  bits<11> Imm;
  let hasSideEffects = 1;

  let Inst{13-10} = Rd;
  let Inst{17-14} = Rs1;
  let Inst{18}    = 1; // Immediate mode
  let Inst{19}    = 0; // Fix disassembly collision with EZHInstALUIShifted
  let Inst{31}    = invert;
  let Inst{30-20} = Imm;
}

// Format for Flip ALU instructions
class EZHInstFlipALU<bits<3> alu_op, bits<1> is_asr, bits<1> is_fbit, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHALUInst<outs, ins, asmstr, pattern> {
  let Opcode = OP_FLIP;
  bits<4> Rd;
  bits<4> Rs1;
  bits<4> Rs2;
  bits<5> ShAmt;
  let hasSideEffects = 1;

  let Inst{13-10} = Rd;
  let Inst{17-14} = Rs1;
  let Inst{23-20} = Rs2;
  let Inst{28-24} = ShAmt;
  let Inst{31-29} = alu_op;
  let Inst{19}    = is_asr;
  let Inst{18}    = is_fbit;
}

class EZHInstALUI12<bits<5> op, bits<1> invert, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHALUInst<outs, ins, asmstr, pattern> {
  let Opcode = op;
  bits<4> Rd;
  bits<4> Rs1;
  bits<12> Imm;
  let hasSideEffects = 1;

  let Inst{13-10} = Rd;
  let Inst{17-14} = Rs1;
  let Inst{18}    = 1; // Immediate mode
  let Inst{19}    = invert;
  let Inst{31-20} = Imm;
}

class EZHInstALUIShifted<bits<5> op, bits<1> invert, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHALUInst<outs, ins, asmstr, pattern> {
  let Opcode = op;
  bits<4> Rd;
  bits<5> ShAmt;
  bits<11> Imm;

  let Inst{13-10} = Rd;
  let Inst{17-14} = ShAmt{3-0};
  let Inst{18}    = 1; // Immediate mode
  let Inst{19}    = ShAmt{4};
  let Inst{30-20} = Imm;
  let Inst{31}    = invert;
}

// Load/Store format
class EZHInstMem<bits<5> op, bits<1> isStore, bits<1> wordSize, bits<1> signedAccess, bits<1> prePost, bits<1> update, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHCondInst<outs, ins, asmstr, pattern> {
  let Opcode = op;
  bits<4> Rd;
  bits<4> Rn;
  bits<8> Offset;
  bits<1> PrePost = prePost;
  bits<1> Update = update;

  let Inst{13-10} = !if(isStore, {0, 0, 0, Update}, Rd);
  let Inst{17-14} = Rn;
  let Inst{18}    = wordSize;
  let Inst{19}    = PrePost;
  let Inst{23-20} = !if(isStore, Rd, {0, 0, signedAccess, Update});
  let Inst{31-24} = Offset;
  let Inst{9}     = 0; // Force S bit to 0

  let isWordMem = wordSize;
}

// Format for ANDOR
class EZHInstAndOr<dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHALUInst<outs, ins, asmstr, pattern> {
  let Opcode = OP_ANDOR;
  bits<4> Rd;
  bits<4> Rs1;
  bits<4> Rs2;
  bits<4> Rs3;

  let Inst{13-10} = Rd;
  let Inst{17-14} = Rs1;
  let Inst{23-20} = Rs2;
  let Inst{27-24} = Rs3;
}

// Format for Bit Manipulation
class EZHInstBit<bits<2> op_type, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHALUInst<outs, ins, asmstr, pattern> {
  let Opcode = OP_BIT;
  bits<4> Rd;
  bits<4> Rs;
  bits<5> Bit5;
  bits<1> S_bit;

  let Inst{13-10} = Rd;
  let Inst{17-14} = Rs;
  let Inst{18}    = 0;
  let Inst{23-19} = 0;
  let Inst{28-24} = Bit5;
  let Inst{30-29} = op_type;
  let Inst{31}    = S_bit; // 1 for BTST, BSET, BTOG. 0 for BCLR.
}

class EZHInstBitReg<bits<2> op_type, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHALUInst<outs, ins, asmstr, pattern> {
  let Opcode = OP_BIT;
  bits<4> Rd;
  bits<4> Rs;
  bits<4> Rbit;
  bits<1> S_bit;

  let Inst{13-10} = Rd;
  let Inst{17-14} = Rs;
  let Inst{23-20} = Rbit;
  let Inst{18}    = 1;
  let Inst{19}    = 0;
  let Inst{28-24} = 0;
  let Inst{30-29} = op_type;
  let Inst{31}    = S_bit; // 1 for BTST, BSET, BTOG. 0 for BCLR.
}

// Format for Register Shift (RLSL, RLSR, RASR, RROR)
class EZHInstRegShift<bits<2> sh_type, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHALUInst<outs, ins, asmstr, pattern> {
  let Opcode = OP_REG_SHIFT;
  bits<4> Rd;
  bits<4> Rs;
  bits<5> Rshift; // Only using 4 bits for register, but macro takes 5

  let Inst{13-10} = Rd;
  let Inst{23-20} = Rs;
  let Inst{19-18} = sh_type;
  let Inst{28-24} = Rshift;
}

// Format for Tight Loop
class EZHInstTightLoop<dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHCondInst<outs, ins, asmstr, pattern> {
  let Opcode = OP_LOOP;
  bits<4> Rend;
  bits<4> Rcount;

  let Inst{17-14} = Rend;
  let Inst{23-20} = Rcount;
  let Inst{9}     = 0; // Force S bit to 0
}

// Format for Modify GPO Byte
class EZHInstModifyGPOByte<dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHInst<outs, ins, asmstr, pattern> {
  let Opcode = OP_MODIFY_GPO;
  bits<8> AndMask;
  bits<8> OrMask;
  bits<8> XorMask;

  let Inst{15-8}  = AndMask;
  let Inst{23-16} = OrMask;
  let Inst{31-24} = XorMask;
}
class EZHInstSingle<bits<5> op, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHInst<outs, ins, asmstr, pattern> {
  let Opcode = op;
}

// Reg Offset Load/Store
class EZHInstMemReg<bits<5> op, bits<1> isStore, bits<1> wordSize, bits<1> signedAccess, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHCondInst<outs, ins, asmstr, pattern> {
  let Opcode = op;
  bits<4> Rd;
  bits<4> Rn;
  bits<4> Rm;

  let Inst{13-10} = !if(isStore, 0, Rd);
  let Inst{17-14} = Rn; // source/raddr
  let Inst{23-20} = !if(isStore, Rd, {0, 0, signedAccess, 0}); // rdata for store
  let Inst{27-24} = Rm; // roffset
  let Inst{18}    = wordSize;
  let Inst{30}    = isStore;
  let Inst{9}     = 0; // Force S bit to 0

  let isWordMem = wordSize;
}

// Format for CALL
class EZHInstCall<bits<5> op, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHInst<outs, ins, asmstr, pattern> {
  let Opcode = op;
  bits<30> addr;
  let Inst{31-2} = addr;
  let Inst{1-0} = 3;
}

// Goto format
class EZHInstGoto<bits<1> isL, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHCondInst<outs, ins, asmstr, pattern> {
  let Opcode = OP_GOTO;
  bits<21> addr;
  let Inst{9} = 1;
  let Inst{10} = isL;
  let Inst{31-11} = addr;
}

class EZHInstGotoReg<bits<1> isL, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHCondInst<outs, ins, asmstr, pattern> {
  let Opcode = OP_GOTO;
  bits<4> raddr;
  let Inst{9} = 0;
  let Inst{10} = isL;
  let Inst{17-14} = raddr;
  let Inst{31-18} = 0;
  let Inst{13-11} = 0;
}

// Shift format
class EZHInstShift<bits<2> sh_type, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHALUInst<outs, ins, asmstr, pattern> {
  let Opcode = OP_SHIFT;
  bits<4> Rd;
  bits<4> Rs;
  bits<5> ShAmt;

  let Inst{13-10} = Rd;
  let Inst{23-20} = Rs;
  let Inst{19-18} = sh_type;
  let Inst{28-24} = ShAmt;
  let Inst{31-29} = 0;
  let Inst{17-14} = 0;
}

// Per Read format
class EZHInstPerRead<bits<5> op, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHCondInst<outs, ins, asmstr, pattern> {
  let Opcode = op;
  bits<4> Rd;
  bits<20> addr;
  let Inst{13-10} = Rd;
  let Inst{31-14} = addr{19-2};
  let Inst{9}     = 0; // Force S bit to 0
}

// Per Write format
class EZHInstPerWrite<bits<5> op, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHCondInst<outs, ins, asmstr, pattern> {
  let Opcode = op;
  bits<4> Rs;
  bits<20> addr;
  let Inst{23-20} = Rs;
  let Inst{31-24} = addr{19-12};
  let Inst{19-10} = addr{11-2};
  let Inst{9}     = 0; // Force S bit to 0
}

class EZHInstShiftALU<bits<3> alu_op, bits<2> sh_type, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHALUInst<outs, ins, asmstr, pattern> {
  let Opcode = OP_SHIFT_ALU;
  bits<4> Rd;
  bits<4> Rs1;
  bits<4> Rs2;
  bits<5> ShAmt;

  let Inst{13-10} = Rd;
  let Inst{17-14} = Rs1;
  let Inst{23-20} = Rs2;
  let Inst{28-24} = ShAmt;
  let Inst{19}    = sh_type{0}; // LSR/ROR is 1, LSL/ASR is 0
  let Inst{18}    = sh_type{1}; // ASR/ROR is 1, LSL/LSR is 0
  let Inst{31-29} = alu_op;
}

class EZHInstRegShiftALU<bits<3> alu_op, bits<2> sh_type, bits<1> is_post, bits<1> is_inv, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHALUInst<outs, ins, asmstr, pattern> {
  let Opcode = OP_REG_SHIFT;
  bits<4> Rd;
  bits<4> Rs1;
  bits<4> Rs2;
  bits<3> Rshift;

  let Inst{13-10} = Rd;
  let Inst{17-14} = Rs1;
  let Inst{23-20} = Rs2;
  let Inst{26-24} = Rshift;
  let Inst{27}    = is_inv;
  let Inst{28}    = is_post;
  let Inst{19}    = sh_type{0}; // LSR/ROR is 1, LSL/ASR is 0
  let Inst{18}    = sh_type{1}; // ASR/ROR is 1, LSL/LSR is 0
  let Inst{31-29} = alu_op;
}

class EZHInstVectoredHold<bits<1> is_nra, bits<1> is_lv, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHCondInst<outs, ins, asmstr, pattern> {
  let Opcode = OP_HOLD;
  bits<4> table;
  let Inst{13-10} = table;
  let Inst{23-20} = table;
  let Inst{15} = 0;
  let Inst{18} = is_nra;
  let Inst{19} = is_lv;
  let Inst{17-16} = 0;
  let Inst{14} = 0;
  let Inst{31-24} = 0;
  let Inst{9}     = 0; // Force S bit to 0
}

class EZHInstAccVectoredHold<bits<1> is_nra, bits<1> is_lv, dag outs, dag ins, string asmstr, list<dag> pattern>
    : EZHCondInst<outs, ins, asmstr, pattern> {
  let Opcode = OP_HOLD;
  bits<4> table;
  bits<8> vectors;
  let Inst{13-10} = table;
  let Inst{23-20} = table;
  let Inst{31-24} = vectors;
  let Inst{15} = 0;
  let Inst{18} = is_nra;
  let Inst{19} = is_lv;
  let Inst{17-16} = 0;
  let Inst{14} = 0;
  let Inst{9} = 1;
}

//===----------------------------------------------------------------------===//
// EZH Predicate Operand
//===----------------------------------------------------------------------===//

def CondCodeOperand : AsmOperandClass {
  let Name = "CondCode";
  let DefaultMethod = "defaultCondCodeOp";
  let IsOptional = true;
}

def pred : PredicateOperand<i32, (ops i32imm), (ops (i32 0))> { // 0 = Cond_eu (Always)
  let PrintMethod = "printPredicateOperand";
  let ParserMatchClass = CondCodeOperand;
  let DecoderMethod = "DecodePredicateOperand";
}
