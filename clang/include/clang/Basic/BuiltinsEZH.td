//===--- BuiltinsEZH.td - EZH Builtin function database ----*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the EZH-specific builtin function database. Each builtin is
// lowered automatically to the matching ezh intrinsic (declared with
// ClangBuiltin in llvm/include/llvm/IR/IntrinsicsEZH.td) by the generic
// ClangBuiltin path in CGBuiltin -- no target-specific CodeGen is required.
//
//===----------------------------------------------------------------------===//

include "clang/Basic/BuiltinsBase.td"

// --- Event / control fabric -------------------------------------------------

def EZHHold : TargetBuiltin {
  let Spellings = ["__builtin_ezh_hold"];
  let Attributes = [NoThrow];
  let Prototype = "void()";
}

// The tight_loop (OP_LOOP) hardware loop: a FREE-RUNNING counted loop. The
// instruction immediately after it is a run-once slot (executed once, not
// repeated; conventionally a nop); the repeated block starts one instruction
// later, ends at rend (normally `&&label` right after the body), and runs
// rcount+1 times with no event pacing. Expert primitive -- see
// IntrinsicsEZH.td for the body contract and layout hazards, and
// ezh/TIGHT_LOOP_TIMING.md for measured timing and the hand-code checklist.
def EZHTightLoop : TargetBuiltin {
  let Spellings = ["__builtin_ezh_tight_loop"];
  let Attributes = [NoThrow];
  let Prototype = "void(void*, unsigned int)";
}

def EZHIntTrigger : TargetBuiltin {
  let Spellings = ["__builtin_ezh_int_trigger"];
  let Attributes = [NoThrow];
  let Prototype = "void(_Constant unsigned int)";
}

def EZHReadCFM : TargetBuiltin {
  let Spellings = ["__builtin_ezh_read_cfm"];
  let Attributes = [NoThrow];
  let Prototype = "unsigned int()";
}
def EZHWriteCFM : TargetBuiltin {
  let Spellings = ["__builtin_ezh_write_cfm"];
  let Attributes = [NoThrow];
  let Prototype = "void(unsigned int)";
}

// CFM single-bit set/clear (1-instruction slice arm/disarm). Bit must be a
// constant in [0,31] (range-checked in Sema). A CFM bit-op does NOT clear the
// sticky bitslice flags (only a full write_cfm does) -- see IntrinsicsEZH.td.
def EZHCFMBSet : TargetBuiltin {
  let Spellings = ["__builtin_ezh_cfm_bset"];
  let Attributes = [NoThrow];
  let Prototype = "void(_Constant unsigned int)";
}
def EZHCFMBClr : TargetBuiltin {
  let Spellings = ["__builtin_ezh_cfm_bclr"];
  let Attributes = [NoThrow];
  let Prototype = "void(_Constant unsigned int)";
}

def EZHReadCFS : TargetBuiltin {
  let Spellings = ["__builtin_ezh_read_cfs"];
  let Attributes = [NoThrow];
  let Prototype = "unsigned int()";
}
def EZHWriteCFS : TargetBuiltin {
  let Spellings = ["__builtin_ezh_write_cfs"];
  let Attributes = [NoThrow];
  let Prototype = "void(unsigned int)";
}

def EZHWaitForBeat : TargetBuiltin {
  let Spellings = ["__builtin_ezh_wait_for_beat"];
  let Attributes = [NoThrow];
  let Prototype = "void()";
}
def EZHSynchAllToBeat : TargetBuiltin {
  let Spellings = ["__builtin_ezh_synch_all_to_beat"];
  let Attributes = [NoThrow];
  let Prototype = "void(_Constant unsigned int)";
}
def EZHHeartRythm : TargetBuiltin {
  let Spellings = ["__builtin_ezh_heart_rythm"];
  let Attributes = [NoThrow];
  let Prototype = "void(unsigned int)";
}
def EZHHeartRythmImm : TargetBuiltin {
  let Spellings = ["__builtin_ezh_heart_rythm_imm"];
  let Attributes = [NoThrow];
  let Prototype = "void(_Constant unsigned int)";
}

// Single-cycle nop, for cycle-exact padding.
def EZHNop : TargetBuiltin {
  let Spellings = ["__builtin_ezh_nop"];
  let Attributes = [NoThrow];
  let Prototype = "void()";
}

// Vectored hold (ACC form): blocks until a bitslice event; returns the
// hardware-computed vector address (table + 4 + 4*slice) when the winning
// slice is enabled in the constant mask. Dispatch through the result with an
// inline-asm `mov pc` -- NEVER a C computed goto, which lets the optimizer
// assume an in-function label target and delete the indirection. See
// IntrinsicsEZH.td for the full contract.
def EZHAccVectoredHold : TargetBuiltin {
  let Spellings = ["__builtin_ezh_acc_vectored_hold"];
  let Attributes = [NoThrow];
  let Prototype = "void*(void*, _Constant unsigned int)";
}

// --- GPO / GPI / GPD --------------------------------------------------------

// GPO read-modify-write: all three masks are compile-time 8-bit constants.
def EZHModifyGPOByte : TargetBuiltin {
  let Spellings = ["__builtin_ezh_modify_gpo_byte"];
  let Attributes = [NoThrow];
  let Prototype = "void(_Constant unsigned int, _Constant unsigned int, _Constant unsigned int)";
}

// Full-width GPO output register read/write (runtime values, all 32 bits).
def EZHReadGPO : TargetBuiltin {
  let Spellings = ["__builtin_ezh_read_gpo"];
  let Attributes = [NoThrow];
  let Prototype = "unsigned int()";
}
def EZHWriteGPO : TargetBuiltin {
  let Spellings = ["__builtin_ezh_write_gpo"];
  let Attributes = [NoThrow];
  let Prototype = "void(unsigned int)";
}

// Full-width GPD drive-enable write (atomic multi-line open-drain update).
def EZHWriteGPD : TargetBuiltin {
  let Spellings = ["__builtin_ezh_write_gpd"];
  let Attributes = [NoThrow];
  let Prototype = "void(unsigned int)";
}

// Read the GPI input register.
def EZHReadGPI : TargetBuiltin {
  let Spellings = ["__builtin_ezh_read_gpi"];
  let Attributes = [NoThrow];
  let Prototype = "unsigned int()";
}

// Read the GPD drive-enable register.
def EZHReadGPD : TargetBuiltin {
  let Spellings = ["__builtin_ezh_read_gpd"];
  let Attributes = [NoThrow];
  let Prototype = "unsigned int()";
}

// Pull an open-drain line low: set GPD bit N. N must be a constant in [0,31]
// (range-checked in Sema; the imm5 encoder is a backstop).
def EZHGPDDriveLow : TargetBuiltin {
  let Spellings = ["__builtin_ezh_gpd_drive_low"];
  let Attributes = [NoThrow];
  let Prototype = "void(_Constant unsigned int)";
}

// Release an open-drain line: clear GPD bit N.
def EZHGPDRelease : TargetBuiltin {
  let Spellings = ["__builtin_ezh_gpd_release"];
  let Attributes = [NoThrow];
  let Prototype = "void(_Constant unsigned int)";
}
