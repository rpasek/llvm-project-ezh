//===- IntrinsicsEZH.td - Defines EZH intrinsics -----------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the EZH-specific intrinsics: the event/control fabric of
// the SmartDMA EZH core (hold, software event triggering, the bitslice
// configuration/status registers CFM and CFS, the beat/heart-rythm timing
// primitives) and the GPIO surface (GPI/GPD/GPO). Each carries side effects so
// the scheduler keeps it ordered against the rest of the I/O. The matching
// clang builtins are wired automatically through the ClangBuiltin association
// (no target-specific CodeGen).
//
//===----------------------------------------------------------------------===//

let TargetPrefix = "ezh" in {

// --- Event / control fabric -------------------------------------------------

// Stop executing until a bitslice event arrives (hold).
def int_ezh_hold : ClangBuiltin<"__builtin_ezh_hold">,
    Intrinsic<[], [], [IntrHasSideEffects]>;

// Raise the software event/interrupt encoded by the immediate (int_trigger).
def int_ezh_int_trigger : ClangBuiltin<"__builtin_ezh_int_trigger">,
    Intrinsic<[], [llvm_i32_ty], [IntrHasSideEffects, ImmArg<ArgIndex<0>>]>;

// Bitslice configuration/mask register CFM (special reg 11).
def int_ezh_read_cfm : ClangBuiltin<"__builtin_ezh_read_cfm">,
    Intrinsic<[llvm_i32_ty], [], [IntrHasSideEffects]>;
def int_ezh_write_cfm : ClangBuiltin<"__builtin_ezh_write_cfm">,
    Intrinsic<[], [llvm_i32_ty], [IntrHasSideEffects]>;

// Bitslice source-select register CFS (special reg 10).
def int_ezh_read_cfs : ClangBuiltin<"__builtin_ezh_read_cfs">,
    Intrinsic<[llvm_i32_ty], [], [IntrHasSideEffects]>;
def int_ezh_write_cfs : ClangBuiltin<"__builtin_ezh_write_cfs">,
    Intrinsic<[], [llvm_i32_ty], [IntrHasSideEffects]>;

// Beat / heart-rythm timing primitives.
def int_ezh_wait_for_beat : ClangBuiltin<"__builtin_ezh_wait_for_beat">,
    Intrinsic<[], [], [IntrHasSideEffects]>;
def int_ezh_synch_all_to_beat : ClangBuiltin<"__builtin_ezh_synch_all_to_beat">,
    Intrinsic<[], [llvm_i32_ty], [IntrHasSideEffects, ImmArg<ArgIndex<0>>]>;
def int_ezh_heart_rythm : ClangBuiltin<"__builtin_ezh_heart_rythm">,
    Intrinsic<[], [llvm_i32_ty], [IntrHasSideEffects]>;
def int_ezh_heart_rythm_imm : ClangBuiltin<"__builtin_ezh_heart_rythm_imm">,
    Intrinsic<[], [llvm_i32_ty], [IntrHasSideEffects, ImmArg<ArgIndex<0>>]>;

// --- GPO / GPI / GPD --------------------------------------------------------

// Atomic read-modify-write of the GPO output register: GPO = (GPO & And) | Or
// ^ Xor, all three masks compile-time 8-bit constants.
def int_ezh_modify_gpo_byte : ClangBuiltin<"__builtin_ezh_modify_gpo_byte">,
    Intrinsic<[], [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
              [IntrHasSideEffects, ImmArg<ArgIndex<0>>, ImmArg<ArgIndex<1>>,
               ImmArg<ArgIndex<2>>]>;

// Read the GPI input register (special reg 14) -> mov $Rd, gpi.
def int_ezh_read_gpi : ClangBuiltin<"__builtin_ezh_read_gpi">,
    Intrinsic<[llvm_i32_ty], [], [IntrHasSideEffects]>;

// Read the GPD drive-enable register (special reg 9) -> mov $Rd, gpd.
def int_ezh_read_gpd : ClangBuiltin<"__builtin_ezh_read_gpd">,
    Intrinsic<[llvm_i32_ty], [], [IntrHasSideEffects]>;

// Open-drain line control via the GPD drive-enable register: set bit N to pull
// the pin low, clear bit N to release it (float high via the external pull-up).
// The bit position is a compile-time constant in [0,31]. GPD is hardwired as
// both source and destination of a single bit-op.
def int_ezh_gpd_drive_low : ClangBuiltin<"__builtin_ezh_gpd_drive_low">,
    Intrinsic<[], [llvm_i32_ty], [IntrHasSideEffects, ImmArg<ArgIndex<0>>]>;
def int_ezh_gpd_release : ClangBuiltin<"__builtin_ezh_gpd_release">,
    Intrinsic<[], [llvm_i32_ty], [IntrHasSideEffects, ImmArg<ArgIndex<0>>]>;

} // TargetPrefix = "ezh"
