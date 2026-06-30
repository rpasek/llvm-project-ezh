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

// --- GPO / GPI / GPD --------------------------------------------------------

// GPO read-modify-write: all three masks are compile-time 8-bit constants.
def EZHModifyGPOByte : TargetBuiltin {
  let Spellings = ["__builtin_ezh_modify_gpo_byte"];
  let Attributes = [NoThrow];
  let Prototype = "void(_Constant unsigned int, _Constant unsigned int, _Constant unsigned int)";
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
// (the range is enforced by the imm5 encoder / the intrinsic's ImmArg).
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
