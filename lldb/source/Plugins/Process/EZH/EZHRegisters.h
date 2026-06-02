//===-- EZHRegisters.h ------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Description:
//   Defines NXP SmartDMA / EZH peripheral AHB slave register offsets, the
//   68-byte stack frame memory dump layout, and LLDB register index IDs.
//
//===----------------------------------------------------------------------===//
#ifndef LLDB_SYMBOL_EZHREGISTERS_H
#define LLDB_SYMBOL_EZHREGISTERS_H

// NXP SmartDMA (EZH) Peripheral Register Offsets
#define EZHB_BOOT_OFFSET        0x20
#define EZHB_CTRL_OFFSET        0x24
#define EZHB_PC_OFFSET          0x28
#define EZHB_SP_OFFSET          0x2C
#define EZHB_BREAK_ADDR_OFFSET  0x30
#define EZHB_BREAK_VECT_OFFSET  0x34
#define EZHB_EMER_VECT_OFFSET   0x38
#define EZHB_EMER_SEL_OFFSET    0x3C
#define EZHB_ARM2EZH_OFFSET     0x40
#define EZHB_EZH2ARM_OFFSET     0x44
#define EZHB_PENDTRAP_OFFSET    0x48

// EZHB_CTRL Register Bit Positions (matching AN14650.txt)
#define EZHB_START              0
#define EZHB_CTRL_WRITE_KEY     0xC0DE0000

// EZHB_PENDTRAP Register Bit Positions (matching AN14650.txt Section 3.1)
#define EZHB_PENDTRAP_EN7       23
#define EZHB_PENDTRAP_REQ7      7

#ifdef __cplusplus

#include <cstdint>

enum EZHOpcode : uint32_t {
  EZH_OPC_MOV = 0x00,
  EZH_OPC_LDR = 0x01,
  EZH_OPC_STR = 0x02,
  EZH_OPC_GOSUB = 0x03,
  EZH_OPC_PER_READ = 0x04,
  EZH_OPC_PER_WRITE = 0x05,
  EZH_OPC_ADD = 0x06,
  EZH_OPC_SUB = 0x08,
  EZH_OPC_ADC = 0x09,
  EZH_OPC_SBC = 0x0A,
  EZH_OPC_OR = 0x0C,
  EZH_OPC_AND = 0x0D,
  EZH_OPC_XOR = 0x0E,
  EZH_OPC_CALL = 0x10,
  EZH_OPC_FLIP = 0x11,
  EZH_OPC_NOP = 0x12,
  EZH_OPC_INT_TRIGGER = 0x14,
  EZH_OPC_GOTO = 0x15,
  EZH_OPC_ANDOR = 0x16,
  EZH_OPC_BIT = 0x18,
  EZH_OPC_REG_SHIFT = 0x19,
  EZH_OPC_TIGHT_LOOP = 0x1A,
  EZH_OPC_HOLD = 0x1C,
  EZH_OPC_MEM_REG = 0x1D,
  EZH_OPC_MODIFY_GPO = 0x1E
};

// EZH Hardware Condition Codes (mirroring llvm/lib/Target/EZH/EZHCondCode.h)
enum EZHCondCode {
  EZH_CC_EU  = 0,  // Execute Unconditionally
  EZH_CC_ZE  = 1,  // Zero (EQ)
  EZH_CC_NZ  = 2,  // Not Zero (NE)
  EZH_CC_PO  = 3,  // Positive (PL/GE)
  EZH_CC_NE  = 4,  // Negative (MI)
  EZH_CC_AZ  = 5,  // Above zero (GT)
  EZH_CC_ZB  = 6,  // Zero or below (LE)
  EZH_CC_CA  = 7,  // Carry set (CS)
  EZH_CC_NC  = 8,  // Carry not set (CC)
  EZH_CC_CZ  = 9,  // Carry set and zero
  EZH_CC_SPO = 10, // Shift-only-when-Positive
  EZH_CC_SNE = 11, // Shift-only-when-Negative
  EZH_CC_NBS = 12, // Not Boolean-expression set
  EZH_CC_NEX = 13, // External flag is not set
  EZH_CC_BS  = 14, // Boolean-expression set
  EZH_CC_EX  = 15  // External flag is set
};

enum EZHShiftType : uint32_t {
  EZH_SHIFT_TYPE_LSL = 0,
  EZH_SHIFT_TYPE_ASR = 1,
  EZH_SHIFT_TYPE_LSR = 2,
  EZH_SHIFT_TYPE_ROR = 3
};

enum EZHBitOpType : uint32_t {
  EZH_BIT_OP_CLEAR  = 0,
  EZH_BIT_OP_SET    = 1,
  EZH_BIT_OP_TOGGLE = 2,
  EZH_BIT_OP_NONE   = 3
};
#endif

// EZH 68-byte Stack Frame Register Layout
#define EZH_FRAME_SIZE          68

#define EZH_FRAME_OFFSET_R0     (-68)
#define EZH_FRAME_OFFSET_R1     (-64)
#define EZH_FRAME_OFFSET_R2     (-60)
#define EZH_FRAME_OFFSET_R3     (-56)
#define EZH_FRAME_OFFSET_R4     (-52)
#define EZH_FRAME_OFFSET_R5     (-48)
#define EZH_FRAME_OFFSET_R6     (-44)
#define EZH_FRAME_OFFSET_R7     (-40)
#define EZH_FRAME_OFFSET_GPO    (-36)
#define EZH_FRAME_OFFSET_GPD    (-32)
#define EZH_FRAME_OFFSET_CFS    (-28)
#define EZH_FRAME_OFFSET_CFM    (-24)
#define EZH_FRAME_OFFSET_SP     (-20)
#define EZH_FRAME_OFFSET_PC     (-16)
#define EZH_FRAME_OFFSET_GPI    (-12)
#define EZH_FRAME_OFFSET_RA     (-8)
#define EZH_FRAME_OFFSET_FLAGS  (-4)

// EZH Instruction Bit Shifts & Masks (matching fsl_smartdma_prv.h)
#define EZH_OPC_MASK_5BIT               0x1F
#define EZH_OPC_MASK_2BIT               0x03
#define EZH_REG_MASK_4BIT               0x0F
#define EZH_COND_MASK_4BIT              0x0F

// LLDB register index defines
#ifdef __cplusplus
enum EZHRegIndex : uint32_t {
  EZH_REG_IDX_R0 = 0,
  EZH_REG_IDX_R1 = 1,
  EZH_REG_IDX_R2 = 2,
  EZH_REG_IDX_R3 = 3,
  EZH_REG_IDX_R4 = 4,
  EZH_REG_IDX_R5 = 5,
  EZH_REG_IDX_R6 = 6,
  EZH_REG_IDX_R7 = 7,
  EZH_REG_IDX_GPO = 8,
  EZH_REG_IDX_GPD = 9,
  EZH_REG_IDX_CFS = 10,
  EZH_REG_IDX_CFM = 11,
  EZH_REG_IDX_SP = 12,
  EZH_REG_IDX_PC = 13,
  EZH_REG_IDX_GPI = 14,
  EZH_REG_IDX_RA = 15,
  EZH_REG_IDX_FLAGS = 16,
  EZH_NUM_REGS = 17
};
#else
#define EZH_REG_IDX_R0          0
#define EZH_REG_IDX_R1          1
#define EZH_REG_IDX_R2          2
#define EZH_REG_IDX_R3          3
#define EZH_REG_IDX_R4          4
#define EZH_REG_IDX_R5          5
#define EZH_REG_IDX_R6          6
#define EZH_REG_IDX_R7          7
#define EZH_REG_IDX_GPO         8
#define EZH_REG_IDX_GPD         9
#define EZH_REG_IDX_CFS         10
#define EZH_REG_IDX_CFM         11
#define EZH_REG_IDX_SP          12
#define EZH_REG_IDX_PC          13
#define EZH_REG_IDX_GPI         14
#define EZH_REG_IDX_RA          15
#define EZH_REG_IDX_FLAGS       16
#define EZH_NUM_REGS            17
#endif

#ifdef __cplusplus
#include <stdint.h>
#define EZH_PAGE_OFFSET_MASK_21BIT      0x1FFFFF
#define EZH_PAGE_BASE_MASK_21BIT        0xFF800000
#define EZH_WORD_ALIGN_MASK_30BIT       0xFFFFFFFC
#define EZH_IMM11_MASK                  0x07FF
#define EZH_IMM11_SIGN_BIT              0x0400
#define EZH_PER_ADDR_MASK_20BIT         0x000FFFFC

#define EZH_INSTR_REG_DEST_SHIFT        10
#define EZH_INSTR_REG_SRC_SHIFT         14
#define EZH_INSTR_REG_SRC3_SHIFT        24
#define EZH_INSTR_IMM_ADDR_SHIFT        11
#define EZH_INSTR_IMM_MATH_SHIFT        20
#define EZH_INSTR_PER_ADDR_SHIFT        12
#define EZH_INSTR_LDR_OFFSET_SHIFT      24
#define EZH_WORD_TO_BYTE_SHIFT          2
#define EZH_IMM_BRANCH_BIT              9
#define EZH_IMM_OPERAND_BIT             18
#define EZH_INVERT_RESULT_BIT           19
#define EZH_INSTR_UPDATE_BIT            19
#define EZH_INSTR_POST_BIT              18
#define EZH_BRANCH_COND_SHIFT           5

// EZH Post-Operation Barrel Shift Modifiers
#define EZH_SHIFT_AMOUNT_SHIFT          24
#define EZH_SHIFT_AMOUNT_MASK           0x1F
#define EZH_SHIFT_TYPE_SHIFT            29
#define EZH_SHIFT_TYPE_MASK             0x07

// EZH Architectural Pipeline & Word Sizes
#define EZH_INSTR_SIZE_BYTES            4
#define EZH_PIPELINE_PC_OFFSET          8
#define EZH_SW_BP_MAGIC_PC_MASK         0xFFFFFFF0
#define EZH_SW_BP_SLOT_MASK             0x0F


// Encodes unconditional branch opcode directly from physical byte address
constexpr uint32_t EncodeGoto(uint32_t phys_addr) {
  uint32_t page_offset =
      (phys_addr >> EZH_WORD_TO_BYTE_SHIFT) & EZH_PAGE_OFFSET_MASK_21BIT;
  return (EZH_OPC_GOTO | (1 << EZH_IMM_BRANCH_BIT)) |
         (page_offset << EZH_INSTR_IMM_ADDR_SHIFT);
}

// Subroutine Call (gosub)
struct EZHGosub {
  uint32_t target_address;
};
constexpr EZHGosub DecodeGosub(uint32_t inst_val) {
  return {inst_val & EZH_WORD_ALIGN_MASK_30BIT};
}

// Branch (goto / goto_reg)
struct EZHGoto {
  uint32_t cond;
  uint32_t rs1;
  uint32_t imm21;
  bool is_reg;
};
constexpr EZHGoto DecodeGoto(uint32_t inst_val) {
  EZHGoto op{};
  op.cond = (inst_val >> EZH_BRANCH_COND_SHIFT) & EZH_COND_MASK_4BIT;
  op.rs1 = (inst_val >> EZH_INSTR_REG_SRC_SHIFT) & EZH_REG_MASK_4BIT;
  op.imm21 = (inst_val >> EZH_INSTR_IMM_ADDR_SHIFT) &
             EZH_PAGE_OFFSET_MASK_21BIT;
  op.is_reg = ((inst_val & (1 << EZH_IMM_BRANCH_BIT)) == 0);
  return op;
}

// Move (mov)
struct EZHMov {
  uint32_t rs1;
  uint32_t imm11;
  bool is_imm;
};
constexpr EZHMov DecodeMov(uint32_t inst_val) {
  EZHMov op{};
  op.rs1 = (inst_val >> EZH_INSTR_REG_SRC_SHIFT) & EZH_REG_MASK_4BIT;
  int32_t imm = static_cast<int32_t>(
      (inst_val >> EZH_INSTR_IMM_MATH_SHIFT) & EZH_IMM11_MASK);
  if ((imm & EZH_IMM11_SIGN_BIT) != 0)
    imm |= ~EZH_IMM11_MASK;
  op.imm11 = static_cast<uint32_t>(imm);
  op.is_imm = ((inst_val & (1 << EZH_IMM_OPERAND_BIT)) != 0);
  return op;
}

// Load Register (ldr)
struct EZHLdr {
  uint32_t rn;
  int32_t offset;
  bool is_post;
};
constexpr EZHLdr DecodeLdr(uint32_t inst_val) {
  EZHLdr op{};
  op.rn = (inst_val >> EZH_INSTR_REG_SRC_SHIFT) & EZH_REG_MASK_4BIT;
  int8_t offset8 =
      static_cast<int8_t>(inst_val >> EZH_INSTR_LDR_OFFSET_SHIFT);
  op.offset = static_cast<int32_t>(offset8) * EZH_INSTR_SIZE_BYTES;
  bool has_update = ((inst_val & (1 << EZH_INSTR_UPDATE_BIT)) != 0);
  bool is_post_bit = ((inst_val & (1 << EZH_INSTR_POST_BIT)) != 0);
  op.is_post = (has_update && is_post_bit);
  return op;
}

// Store Register (str)
struct EZHStr {
  uint32_t rn;
  uint32_t rt;
  int32_t offset;
  bool is_post;
  bool has_update;
};
constexpr EZHStr DecodeStr(uint32_t inst_val) {
  EZHStr op{};
  op.rn = (inst_val >> EZH_INSTR_REG_SRC_SHIFT) & EZH_REG_MASK_4BIT;
  op.rt = (inst_val >> EZH_INSTR_IMM_MATH_SHIFT) & EZH_REG_MASK_4BIT;
  int8_t offset8 = static_cast<int8_t>(inst_val >> EZH_INSTR_LDR_OFFSET_SHIFT);
  op.offset = static_cast<int32_t>(offset8) * EZH_INSTR_SIZE_BYTES;
  op.has_update = ((inst_val & (1 << 10)) != 0);
  bool is_post_bit = ((inst_val & (1 << 19)) != 0);
  op.is_post = (op.has_update && is_post_bit);
  return op;
}

// Peripheral Read (per_read)
struct EZHPerRead {
  uint32_t per_addr;
};
constexpr EZHPerRead DecodePerRead(uint32_t inst_val) {
  return {(inst_val >> EZH_INSTR_PER_ADDR_SHIFT) & EZH_PER_ADDR_MASK_20BIT};
}

// Peripheral Write (per_write)
struct EZHPerWrite {
  uint32_t rs;
  uint32_t per_addr;
};
constexpr EZHPerWrite DecodePerWrite(uint32_t inst_val) {
  EZHPerWrite op{};
  op.rs = (inst_val >> 20) & 0xF;
  op.per_addr = ((inst_val >> 12) & 0x000FF000U) | ((inst_val >> 8) & 0x00000FFCU);
  return op;
}

// And-then-Or (andor)
struct EZHAndOr {
  uint32_t rs1;
  uint32_t rs2;
  uint32_t rs3;
};
constexpr EZHAndOr DecodeAndOr(uint32_t inst_val) {
  EZHAndOr op{};
  op.rs1 = (inst_val >> EZH_INSTR_REG_SRC_SHIFT) & EZH_REG_MASK_4BIT;
  op.rs2 = (inst_val >> EZH_INSTR_IMM_MATH_SHIFT) & EZH_REG_MASK_4BIT;
  op.rs3 = (inst_val >> EZH_INSTR_REG_SRC3_SHIFT) & EZH_REG_MASK_4BIT;
  return op;
}

// ALU Operations (add, sub, adc, sbc, and, or, xor)
struct EZHAlu {
  uint32_t rs1;
  uint32_t rs2;
  uint32_t imm12;
  bool is_imm;
};
constexpr EZHAlu DecodeAlu(uint32_t inst_val) {
  EZHAlu op{};
  op.rs1 = (inst_val >> EZH_INSTR_REG_SRC_SHIFT) & EZH_REG_MASK_4BIT;
  op.rs2 = (inst_val >> EZH_INSTR_IMM_MATH_SHIFT) & EZH_REG_MASK_4BIT;
  op.imm12 = inst_val >> EZH_INSTR_IMM_MATH_SHIFT;
  op.is_imm = ((inst_val & (1 << EZH_IMM_OPERAND_BIT)) != 0);
  return op;
}

// Register-Offset Memory Access (mem_reg)
struct EZHMemReg {
  uint32_t rn;
  uint32_t rm;
};
constexpr EZHMemReg DecodeMemReg(uint32_t inst_val) {
  EZHMemReg op{};
  op.rn = (inst_val >> EZH_INSTR_REG_SRC_SHIFT) & EZH_REG_MASK_4BIT;
  op.rm = (inst_val >> EZH_INSTR_REG_SRC3_SHIFT) & EZH_REG_MASK_4BIT;
  return op;
}

// Register Shift (reg_shift)
struct EZHRegShift {
  uint32_t rs;
  uint32_t rshift;
  EZHShiftType sh_type;
};
constexpr EZHRegShift DecodeRegShift(uint32_t inst_val) {
  EZHRegShift op{};
  op.rs = (inst_val >> EZH_INSTR_IMM_MATH_SHIFT) & EZH_REG_MASK_4BIT;
  op.rshift = (inst_val >> EZH_INSTR_REG_SRC3_SHIFT) & EZH_REG_MASK_4BIT;
  op.sh_type = static_cast<EZHShiftType>((inst_val >> 18) & 3);
  return op;
}

// Bit Manipulation (bit)
struct EZHBit {
  uint32_t rs;
  uint32_t rbit;
  uint32_t imm5;
  EZHBitOpType op_type;
  bool is_reg;
};
constexpr EZHBit DecodeBit(uint32_t inst_val) {
  EZHBit op{};
  op.rs = (inst_val >> EZH_INSTR_REG_SRC_SHIFT) & EZH_REG_MASK_4BIT;
  op.rbit = (inst_val >> EZH_INSTR_IMM_MATH_SHIFT) & EZH_REG_MASK_4BIT;
  op.imm5 = (inst_val >> EZH_SHIFT_AMOUNT_SHIFT) & EZH_SHIFT_AMOUNT_MASK;
  op.op_type = static_cast<EZHBitOpType>((inst_val >> 29) & 3);
  op.is_reg = ((inst_val & (1 << 18)) != 0);
  return op;
}

// Flip ALU (flip)
struct EZHFlip {
  uint32_t rs;
};
constexpr EZHFlip DecodeFlip(uint32_t inst_val) {
  return {(inst_val >> EZH_INSTR_REG_SRC_SHIFT) & EZH_REG_MASK_4BIT};
}

// Universal Modifiers (invert & shift)
struct EZHModifiers {
  uint32_t shift_amount;
  EZHShiftType shift_type;
  bool invert_result;
};
constexpr EZHModifiers DecodeModifiers(uint32_t inst_val) {
  EZHModifiers op{};
  op.shift_amount =
      (inst_val >> EZH_SHIFT_AMOUNT_SHIFT) & EZH_SHIFT_AMOUNT_MASK;
  op.shift_type = static_cast<EZHShiftType>(
      (inst_val >> EZH_SHIFT_TYPE_SHIFT) & EZH_SHIFT_TYPE_MASK);
  op.invert_result = ((inst_val & (1 << EZH_INVERT_RESULT_BIT)) != 0);
  return op;
}

#endif // __cplusplus

#endif // LLDB_SYMBOL_EZHREGISTERS_H
