//===- IntrinsicsEZH.td - Defines EZH intrinsics -----------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the EZH-specific intrinsics. This first increment covers
// the GPIO surface the SmartDMA bit-banging demos rely on: reading the GPI
// input register and driving/releasing open-drain lines through the GPD
// drive-enable register. Each carries side effects so the scheduler keeps it
// ordered against the other I/O. The matching clang builtins are wired
// automatically through the ClangBuiltin association (no manual CodeGen).
//
//===----------------------------------------------------------------------===//

let TargetPrefix = "ezh" in {

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
