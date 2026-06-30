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
