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

// The tight_loop (OP_LOOP) hardware loop: re-execute the instructions between
// this point and the code address Rend a total of Rcount+1 times, then fall
// through to Rend. Expert primitive with the same contract as hold: the body
// is meant to hold on an armed bitslice event each iteration (event-paced DMA;
// a free-running loop does not iterate). Because the compiler models the body
// as straight-line code that runs once, the body must consist of side-effecting
// operations only (volatile/MMIO accesses and EZH intrinsics), with any
// loop-carried state in volatile storage, and must contain no control flow.
// Rend is normally `&&label` of the label placed immediately after the body.
def int_ezh_tight_loop : ClangBuiltin<"__builtin_ezh_tight_loop">,
    Intrinsic<[], [llvm_ptr_ty, llvm_i32_ty], [IntrHasSideEffects]>;

// Raise the software event/interrupt encoded by the immediate (int_trigger).
def int_ezh_int_trigger : ClangBuiltin<"__builtin_ezh_int_trigger">,
    Intrinsic<[], [llvm_i32_ty], [IntrHasSideEffects, ImmArg<ArgIndex<0>>]>;

// Bitslice configuration/mask register CFM (special reg 11).
def int_ezh_read_cfm : ClangBuiltin<"__builtin_ezh_read_cfm">,
    Intrinsic<[llvm_i32_ty], [], [IntrHasSideEffects]>;
def int_ezh_write_cfm : ClangBuiltin<"__builtin_ezh_write_cfm">,
    Intrinsic<[], [llvm_i32_ty], [IntrHasSideEffects]>;

// CFM single-bit set/clear: bset_imm/bclr_imm with CFM hardwired as both
// source and destination, the 1-instruction slice arm/disarm (NXP's display
// firmware disarms a slice right after every HOLD wake). The bit position is
// a compile-time constant in [0,31]. Note (measured on silicon): unlike a
// full mov-to-CFM write, a CFM bit-op does NOT clear the sticky bitslice
// flags -- using bset as the per-iteration re-arm in an event-paced stream
// loses pacing by one event; keep write_cfm as the flag-clearing re-arm.
def int_ezh_cfm_bset : ClangBuiltin<"__builtin_ezh_cfm_bset">,
    Intrinsic<[], [llvm_i32_ty], [IntrHasSideEffects, ImmArg<ArgIndex<0>>]>;
def int_ezh_cfm_bclr : ClangBuiltin<"__builtin_ezh_cfm_bclr">,
    Intrinsic<[], [llvm_i32_ty], [IntrHasSideEffects, ImmArg<ArgIndex<0>>]>;

// Bitslice source-select register CFS (special reg 10).
def int_ezh_read_cfs : ClangBuiltin<"__builtin_ezh_read_cfs">,
    Intrinsic<[llvm_i32_ty], [], [IntrHasSideEffects]>;
def int_ezh_write_cfs : ClangBuiltin<"__builtin_ezh_write_cfs">,
    Intrinsic<[], [llvm_i32_ty], [IntrHasSideEffects]>;

// Single-cycle nop, for cycle-exact padding in bit-banged waveforms.
def int_ezh_nop : ClangBuiltin<"__builtin_ezh_nop">,
    Intrinsic<[], [], [IntrHasSideEffects]>;

// The vectored hold (ACC form): block until a bitslice event; if the winning
// slice n has bit n set in the constant dispatch-enable mask, return the
// hardware-computed vector address table + 4 + 4*n; a masked-out slice still
// wakes but resumes with an unspecified return value (the spurious-wake
// path -- callers that mask a subset should compare the result against the
// table bounds). The caller dispatches through the returned address (e.g.
// `asm volatile("mov pc, %0" :: "r"(vec))`); each table slot is one
// instruction (typically a goto to the slice's handler). Silicon-validated:
// slice n lands at slot 1+n. The plain (non-ACC) vectored_hold encodes an
// empty mask and never dispatches; it stays assembler-only.
def int_ezh_acc_vectored_hold
    : ClangBuiltin<"__builtin_ezh_acc_vectored_hold">,
      Intrinsic<[llvm_ptr_ty], [llvm_ptr_ty, llvm_i32_ty],
                [IntrHasSideEffects, ImmArg<ArgIndex<1>>]>;

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

// Full-width GPO output register (special reg 8): mov to/from. This is the
// parallel-bus output path (camera/LCD PIO streaming) -- modify_gpo_byte only
// reaches GPO[7:0] with compile-time masks; these carry runtime values across
// the full 32-bit width, and the read enables arbitrary read-modify-write.
def int_ezh_read_gpo : ClangBuiltin<"__builtin_ezh_read_gpo">,
    Intrinsic<[llvm_i32_ty], [], [IntrHasSideEffects]>;
def int_ezh_write_gpo : ClangBuiltin<"__builtin_ezh_write_gpo">,
    Intrinsic<[], [llvm_i32_ty], [IntrHasSideEffects]>;

// Full-width GPD drive-enable write (special reg 9): atomic multi-line
// open-drain update with a runtime mask; complements the constant-bit
// gpd_drive_low/gpd_release pair below.
def int_ezh_write_gpd : ClangBuiltin<"__builtin_ezh_write_gpd">,
    Intrinsic<[], [llvm_i32_ty], [IntrHasSideEffects]>;

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
