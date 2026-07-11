/*
 * Copyright 2026 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "EZHRegisters.h"
#include "ezh.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

typedef int ssize_t; // Seems to be missing from libc

extern char __data_load_start[];
extern char __data_start[];
extern char __data_end[];
extern char __bss_start[];
extern char __bss_end[];

#ifndef STACK_SIZE_WORDS
#define STACK_SIZE_WORDS 256
#endif // STACK_SIZE_WORDS

static uint32_t __attribute__((section(".stack"))) stack[STACK_SIZE_WORDS];

#ifdef __TEST__
extern ssize_t __llvm_libc_stdio_write(void *cookie, const char *buf,
                                       size_t size);

// If a test fails with an exc_signal of 0xCAFEDEAD, it means that nearly no
// code was executed.
volatile int __attribute__((section(".exc_signal"))) exc_signal = 0xCAFEDEAD;
int bss_test_var;

#endif // __TEST__

#ifdef __EZH_BITSLICE_INTERRUPTS__
volatile uint32_t __ezh_debug_frame = 0;
#endif // __EZH_BITSLICE_INTERRUPTS__

void exit(int status);
int main(int argc, char **argv);

static ezh_boot_func_t g_boot_arg = NULL;
ezh_boot_func_t ezh_get_boot_arg(void) {
  return g_boot_arg;
}

void __attribute__((section(".start"), naked)) _start() {
  __asm__ volatile("nop\n"
                   "nop\n"
                   // Disable all bit slice channel except for slice 7 (used for
                   // debug) and connect all to logical combiner
                   "ldr cfm, pc, cfm_const\n"
                   "ldr sp, pc, sp_const\n"
                   "gosub _start_c\n"
                   "sp_const: .long %0\n"
                   "cfm_const: .long %1\n"
                   :
                   : "i"(&stack[sizeof(stack) / sizeof(stack[0])]),
                     "i"(
#ifdef __EZH_BITSLICE_INTERRUPTS__
                         // When bit slice interrupts are enabled, we enable
                         // slice 7 to allow for halting while debugging. Please
                         // see ezh_bs_vector7() below.
                         BS7(BS_SIG) |
#else
                         BS7(BS_0) |
#endif // __EZH_BITSLICE_INTERRUPTS__
                         BS6(BS_0) | BS5(BS_0) | BS4(BS_0) | BS3(BS_0) |
                         BS2(BS_0) | BS1(BS_0) | BS0(BS_0) | 0xFF));
}

void _start_c(ezh_boot_func_t boot_func) {
#ifdef __TEST__
  exc_signal = 0xCAFE5555; // the application made it to _start_c
#endif // __TEST__

  ezh_write_cfs(BS7(EZH_INPUT_SOURCE_7) | BS6(EZH_INPUT_SOURCE_6) |
                BS5(EZH_INPUT_SOURCE_5) | BS4(EZH_INPUT_SOURCE_4) |
                BS3(EZH_INPUT_SOURCE_3) | BS2(EZH_INPUT_SOURCE_2) |
                BS1(EZH_INPUT_SOURCE_1) | BS0(EZH_INPUT_SOURCE_0) | 0x0);

  uint32_t *data_load = (uint32_t *)__data_load_start;
  uint32_t *data_ptr = (uint32_t *)__data_start;
  uint32_t *data_end = (uint32_t *)__data_end;

  while (data_ptr < data_end) {
    *data_ptr++ = *data_load++;
  }

  uint32_t *bss_ptr = (uint32_t *)__bss_start;
  uint32_t *bss_end = (uint32_t *)__bss_end;

#ifdef __TEST__
  bss_test_var = 0x12345678;
#endif // __TEST__
  while (bss_ptr < bss_end) {
    *bss_ptr++ = 0;
  }

#ifdef __TEST__
  exc_signal = 0xCAFE0B55; // BSS cleared
#endif // __TEST__

  // Execute all global C++ static constructors!
  extern void (*__init_array_start[])(void);
  extern void (*__init_array_end[])(void);
  size_t init_size = __init_array_end - __init_array_start;
  for (size_t i = 0; i < init_size; i++) {
    if (__init_array_start[i]) {
      __init_array_start[i]();
    }
  }

  g_boot_arg = boot_func;

  int ret = main(0, NULL);

  exit(ret);
}

void __attribute__((used)) exit(int status) {
  // Execute all global C++ static destructors in reverse order!
  extern void (*__fini_array_start[])(void);
  extern void (*__fini_array_end[])(void);
  size_t fini_size = __fini_array_end - __fini_array_start;
  for (size_t i = fini_size; i > 0; i--) {
    if (__fini_array_start[i - 1]) {
      __fini_array_start[i - 1]();
    }
  }

  // Call __cxa_atexit registered destructors
  extern void __cxa_finalize(void *dso);
  __cxa_finalize(NULL);

  #ifdef __TEST__
  if (status == 0 || status == (int)0xCAFEBABE) {
    exc_signal = 0xCAFEBABE;
    // Print "exit 0\n" directly to stdout_buffer
    __llvm_libc_stdio_write(NULL, "exit 0\n", 7);
  } else {
    exc_signal = status;
    // Print "exit 1\n" directly to stdout_buffer
    __llvm_libc_stdio_write(NULL, "exit 1\n", 7);
  }
#else
  (void)status;
#endif // __TEST__

  while (1) {
    ezh_hold();
  }
}

void abort() {
#ifdef __TEST__
  if ((exc_signal & 0xFFFF0000) == 0xDEAD0000) {
    exit(exc_signal);
  } else {
    exit(0xDEADC0DE);
  }
#else
  exit(1);
#endif // __TEST__
}

// Standard __cxa_atexit / destructors support
#define ATEXIT_MAX_FUNCS 32

typedef void (*destructor_t)(void *);

struct atexit_entry {
  destructor_t dest;
  void *obj;
  void *dso;
};

static struct atexit_entry atexit_funcs[ATEXIT_MAX_FUNCS];
static int atexit_func_count = 0;

void *__dso_handle = &__dso_handle;

int __cxa_atexit(destructor_t dest, void *obj, void *dso) {
  if (atexit_func_count >= ATEXIT_MAX_FUNCS) {
    return -1;
  }
  atexit_funcs[atexit_func_count++] = (struct atexit_entry){dest, obj, dso};
  return 0;
}

void __cxa_finalize(void *dso) {
  for (int i = atexit_func_count - 1; i >= 0; --i) {
    if (atexit_funcs[i].dest && (dso == NULL || atexit_funcs[i].dso == dso)) {
      atexit_funcs[i].dest(atexit_funcs[i].obj);
      atexit_funcs[i].dest = NULL; // Prevent double call
    }
  }
}

#ifdef __EZH_BITSLICE_INTERRUPTS__
/* clang-format off */
void __attribute__((naked, section(".text.bitslice_interrupts")))
__ezh_debug_common() {
    __asm__ volatile (
        /* Note: ra (RA_orig) was already saved by caller! */
        "str sp, gpi, " STR(EZH_FRAME_OFFSET_GPI) "\n"
        "str sp,  ra, " STR(EZH_FRAME_OFFSET_PC) "\n"
        "str sp,  sp, " STR(EZH_FRAME_OFFSET_SP) "\n"
        "str sp, cfm, " STR(EZH_FRAME_OFFSET_CFM) "\n"
        "str sp, cfs, " STR(EZH_FRAME_OFFSET_CFS) "\n"
        "str sp, gpd, " STR(EZH_FRAME_OFFSET_GPD) "\n"
        "str sp, gpo, " STR(EZH_FRAME_OFFSET_GPO) "\n"
        "str sp,  r7, " STR(EZH_FRAME_OFFSET_R7) "\n"
        "str sp,  r6, " STR(EZH_FRAME_OFFSET_R6) "\n"
        "str sp,  r5, " STR(EZH_FRAME_OFFSET_R5) "\n"
        "str sp,  r4, " STR(EZH_FRAME_OFFSET_R4) "\n"
        "str sp,  r3, " STR(EZH_FRAME_OFFSET_R3) "\n"
        "str sp,  r2, " STR(EZH_FRAME_OFFSET_R2) "\n"
        "str sp,  r1, " STR(EZH_FRAME_OFFSET_R1) "\n"
        "str sp,  r0, " STR(EZH_FRAME_OFFSET_R0) "\n"

        /* Build flags register in R0 */
        /* R0 = 1 (bit 0 is EU, always 1) */
        "load_imm r0, 1\n"
        "bset_imm_ze r0, r0, 1\n"
        "bset_imm_nz r0, r0, 2\n"
        "bset_imm_po r0, r0, 3\n"
        "bset_imm_ne r0, r0, 4\n"
        "bset_imm_az r0, r0, 5\n"
        "bset_imm_zb r0, r0, 6\n"
        "bset_imm_ca r0, r0, 7\n"
        "bset_imm_nc r0, r0, 8\n"
        "bset_imm_cz r0, r0, 9\n"
        "bset_imm_spo r0, r0, 10\n"
        "bset_imm_sne r0, r0, 11\n"
        "bset_imm_nbs r0, r0, 12\n"
        "bset_imm_nex r0, r0, 13\n"
        "bset_imm_bs r0, r0, 14\n"
        "bset_imm_ex r0, r0, 15\n"
        /* Save flags to stack */
        "str sp, r0, " STR(EZH_FRAME_OFFSET_FLAGS) "\n"

        /* Load __ezh_debug_frame address into R0 */
        "ldr r0, pc, __ezh_debug_frame_ptr\n"
        /* Write SP to __ezh_debug_frame */
        "str r0, sp, 0\n"

        "poll_debug_frame:\n"
        /* Read __ezh_debug_frame from RAM */
        "ldr r1, r0, 0\n"
        /* Set EZH ALU flags */
        "sub_imms r1, r1, 0\n"
        /* Loop back to poll_debug_frame if __ezh_debug_frame != 0 (NZ = 1) */
        "goto_nz poll_debug_frame\n"

        /* Restore ALU flags before restoring GPRs! */
        "ldr r0, sp, " STR(EZH_FRAME_OFFSET_FLAGS) "\n"

        /*
         * Reconstruct 3-bit mask (ZE at bit 0, PO at bit 1, CA at bit 2)
         * from flags word
         */
        "lsr r1, r0, 1\n"
        /* r1 has ZE at bit 0 */
        "and_imm r1, r1, 1\n"

        /* shift PO (bit 3) to bit 1 (shift by 2) */
        "lsr r2, r0, 2\n"
        /* r2 has PO at bit 1 */
        "and_imm r2, r2, 2\n"
        "or r1, r1, r2\n"

        /* shift CA (bit 7) to bit 2 (shift by 5) */
        "lsr r2, r0, 5\n"
        /* r2 has CA at bit 2 */
        "and_imm r2, r2, 4\n"
        /* r0 has the reconstructed 3-bit mask! */
        "or r0, r1, r2\n"

        /* Pre-initialize r1 to 0 for restoration baseline */
        "load_imm r1, 0\n"

        /* Countdown flag parsing (same as bitslice_handler) */
        /* Subtract 2 to check State 2 (ZE=0, PO=1, CA=0) */
        "sub_imms r0, r0, 2\n"
        "goto_ze debug_restore_state_2\n"

        /* Subtract 1 to check State 3 (ZE=1, PO=1, CA=0) */
        "sub_imms r0, r0, 1\n"
        "goto_ze debug_restore_state_3\n"

        /* Subtract 1 to check State 4 (ZE=0, PO=0, CA=1) */
        "sub_imms r0, r0, 1\n"
        "goto_ze debug_restore_state_4\n"

        /* Subtract 2 (skipping 5) to check State 6 (ZE=0, PO=1, CA=1) */
        "sub_imms r0, r0, 2\n"
        "goto_ze debug_restore_state_6\n"

        /* Subtract 1 to check State 7 (ZE=1, PO=1, CA=1) */
        "sub_imms r0, r0, 1\n"
        "goto_ze debug_restore_state_7\n"

        /* Fallback to State 0 (ZE=0, PO=0, CA=0 -> 0x155) */
        "goto debug_restore_state_0\n"

        "debug_restore_state_2:\n"
        "load_imms r1, 1\n"
        "goto debug_restore_done\n"

        "debug_restore_state_3:\n"
        "load_imms r1, 0\n"
        "goto debug_restore_done\n"

        "debug_restore_state_4:\n"
        "sub_imms r1, r1, 1\n"
        "goto debug_restore_done\n"

        "debug_restore_state_6:\n"
        "sub_imms r1, r1, -1\n"
        "goto debug_restore_done\n"

        "debug_restore_state_7:\n"
        "sub_imm r1, r1, 1\n"
        "add_imms r1, r1, 1\n"
        "goto debug_restore_done\n"

        "debug_restore_state_0:\n"
        "load_imms r1, -1\n"

        "debug_restore_done:\n"

        /* Restore registers */
        "ldr r0,  sp, " STR(EZH_FRAME_OFFSET_R0) "\n"
        "ldr r1,  sp, " STR(EZH_FRAME_OFFSET_R1) "\n"
        "ldr r2,  sp, " STR(EZH_FRAME_OFFSET_R2) "\n"
        "ldr r3,  sp, " STR(EZH_FRAME_OFFSET_R3) "\n"
        "ldr r4,  sp, " STR(EZH_FRAME_OFFSET_R4) "\n"
        "ldr r5,  sp, " STR(EZH_FRAME_OFFSET_R5) "\n"
        "ldr r6,  sp, " STR(EZH_FRAME_OFFSET_R6) "\n"
        "ldr r7,  sp, " STR(EZH_FRAME_OFFSET_R7) "\n"
        "ldr gpo, sp, " STR(EZH_FRAME_OFFSET_GPO) "\n"
        "ldr gpd, sp, " STR(EZH_FRAME_OFFSET_GPD) "\n"
        "ldr cfs, sp, " STR(EZH_FRAME_OFFSET_CFS) "\n"
        "ldr cfm, sp, " STR(EZH_FRAME_OFFSET_CFM) "\n"
        /* GPI restored from debug SP base */
        "ldr gpi, sp, " STR(EZH_FRAME_OFFSET_GPI) "\n"
        /* RA restored from debug SP base */
        "ldr ra,  sp, " STR(EZH_FRAME_OFFSET_RA) "\n"
        /* Restore SP to original base! */
        "ldr sp,  sp, " STR(EZH_FRAME_OFFSET_SP) "\n"
        /* Load PC to jump return! */
        "ldr pc,  sp, " STR(EZH_FRAME_OFFSET_PC) "\n"

        "__ezh_debug_frame_ptr: .long __ezh_debug_frame\n"
    );
}

void __attribute__((naked, section(".text.bitslice_interrupts"))) debug_call() {
    __asm__ volatile (
        // Save return RA (return PC) instantly! (0-Clobber!)
        "str sp,  ra, " STR(EZH_FRAME_OFFSET_RA) "\n"
        // Jump directly to common handler!
        "goto __ezh_debug_common\n"
    );
}

// Weak stubs for bitslice vectors
void __attribute__((weak)) ezh_bs_vector0() {}
void __attribute__((weak)) ezh_bs_vector1() {}
void __attribute__((weak)) ezh_bs_vector2() {}
void __attribute__((weak)) ezh_bs_vector3() {}
void __attribute__((weak)) ezh_bs_vector4() {}
void __attribute__((weak)) ezh_bs_vector5() {}
void __attribute__((weak)) ezh_bs_vector6() {}
void __attribute__((weak)) ezh_bs_vector7() {
    debug_call();
}

void __attribute__((naked, section(".text.bitslice_interrupts")))
__ezh_bitslice_handler() {
    __asm__ volatile (
        /* Save original RA (which is in RA because gotol saves PC to RA) */
        /* Note: The `.cfi_*` directives interspersed below are Call Frame
           Information instructions for the debugger (LLDB) to track stack
           unwinding. They are processed by the assembler to generate debug info
           (.eh_frame) and do NOT generate any physical machine instructions.
        */
        "pushd ra\n .cfi_def_cfa_offset 4\n .cfi_offset 15, -4\n"

        /* Save GPRs and registers we might clobber.
           We save R0-R3 because they are caller-saved in ABI
           and might be clobbered by C functions called from here.
        */
        "pushd r0\n .cfi_def_cfa_offset 8\n .cfi_offset 0, -8\n"
        "pushd r1\n .cfi_def_cfa_offset 12\n .cfi_offset 1, -12\n"
        "pushd r2\n .cfi_def_cfa_offset 16\n .cfi_offset 2, -16\n"
        "pushd r3\n .cfi_def_cfa_offset 20\n .cfi_offset 3, -20\n"

        /* Save CFM configuration */
        "pushd cfm\n .cfi_def_cfa_offset 24\n .cfi_offset 11, -24\n"

        /* Save ALU flags
           We only capture ZE (bit 0), PO (bit 1), and CA (bit 2)
           consecutively to ensure contiguous countdown mask values.
         */
        "load_imm r0, 0\n"
        "bset_imm_ze r0, r0, 0\n"
        "bset_imm_po r0, r0, 1\n"
        "bset_imm_ca r0, r0, 2\n"
        /* Save flags mask */
        "pushd r0\n"
        ".cfi_def_cfa_offset 28\n"

        /*
         * Read CFS to see what bit slices are activated (lower 8 bits) into
         * R1
         */
        "mov r1, cfs\n"

        /* Deactivate all bit slices to prevent recursive bitslice interrupts
           during handler/vector execution.
        */
        "lsr r0, cfm, 8\n"
        "lsl cfm, r0, 8\n"

        /* Check each bit of CFS [7:0] and call active vectors using conditional
           branch with link (gotol_nz). gotol_nz automatically updates RA with
           the return address.
        */

        /* Check bit 0 (vector0) */
        "btst_imms r0, r1, 0\n"
        "gotol_nz ezh_bs_vector0\n"

        /* Check bit 1 (vector1) */
        "btst_imms r0, r1, 1\n"
        "gotol_nz ezh_bs_vector1\n"

        /* Check bit 2 (vector2) */
        "btst_imms r0, r1, 2\n"
        "gotol_nz ezh_bs_vector2\n"

        /* Check bit 3 (vector3) */
        "btst_imms r0, r1, 3\n"
        "gotol_nz ezh_bs_vector3\n"

        /* Check bit 4 (vector4) */
        "btst_imms r0, r1, 4\n"
        "gotol_nz ezh_bs_vector4\n"

        /* Check bit 5 (vector5) */
        "btst_imms r0, r1, 5\n"
        "gotol_nz ezh_bs_vector5\n"

        /* Check bit 6 (vector6) */
        "btst_imms r0, r1, 6\n"
        "gotol_nz ezh_bs_vector6\n"

        /* Check bit 7 (vector7) */
        "btst_imms r0, r1, 7\n"
        "gotol_nz ezh_bs_vector7\n"

        /* Restore ALU flags.
           Load packed flags from stack into R0.
           Pre-initialize R1 to 0 to serve as a zero baseline for flag
           restoration.
        */
        "popd r0\n"
        ".cfi_def_cfa_offset 24\n"
        "load_imm r1, 0\n"

        /* Compressed Countdown flag parsing:
           We progressively decrement R0 in a linear chain, combining skips
           over impossible states into single subtractions of 2.
           Layout: bit 0: ZE, bit 1: PO, bit 2: CA
        */
        /* Subtract 2 to check State 2 (ZE=0, PO=1, CA=0) */
        "sub_imms r0, r0, 2\n"
        "goto_ze restore_state_2\n"

        /* Subtract 1 to check State 3 (ZE=1, PO=1, CA=0) */
        "sub_imms r0, r0, 1\n"
        "goto_ze restore_state_3\n"

        /* Subtract 1 to check State 4 (ZE=0, PO=0, CA=1) */
        "sub_imms r0, r0, 1\n"
        "goto_ze restore_state_4\n"

        /* Subtract 2 (skipping 5) to check State 6 (ZE=0, PO=1, CA=1) */
        "sub_imms r0, r0, 2\n"
        "goto_ze restore_state_6\n"

        /* Subtract 1 to check State 7 (ZE=1, PO=1, CA=1) */
        "sub_imms r0, r0, 1\n"
        "goto_ze restore_state_7\n"

        /* Fallback if initially 0 or invalid */
        "goto restore_state_0\n"

        /*
         * State 0: ZE=0, PO=0, CA=0 -> load_imms r1, -1 (sets ZE=0, PO=0,
         * CA=0)
         */
        "restore_state_0:\n"
        "load_imms r1, -1\n"
        "goto restore_done\n"

        /*
         * State 2: ZE=0, PO=1, CA=0 -> load_imms r1, 1 (sets ZE=0, PO=1,
         * CA=0)
         */
        "restore_state_2:\n"
        "load_imms r1, 1\n"
        "goto restore_done\n"

        /*
         * State 3: ZE=1, PO=1, CA=0 -> load_imms r1, 0 (sets ZE=1, PO=1,
         * CA=0)
         */
        "restore_state_3:\n"
        "load_imms r1, 0\n"
        "goto restore_done\n"

        /* State 4: ZE=0, PO=0, CA=1 -> execute R1 = 0 - 1 */
        "restore_state_4:\n"
        /* R1 is already 0 */
        "sub_imms r1, r1, 1\n"
        "goto restore_done\n"

        /* State 6: ZE=0, PO=1, CA=1 -> execute R1 = 0 - (-1) */
        "restore_state_6:\n"
        /* R1 is already 0 -> 0 - (-1) = 1 */
        "sub_imms r1, r1, -1\n"
        "goto restore_done\n"

        /* State 7: ZE=1, PO=1, CA=1 -> execute R1 = 0xFFFFFFFF + 1 */
        "restore_state_7:\n"
        /* R1 = 0 - 1 = 0xFFFFFFFF (-1) */
        "sub_imm r1, r1, 1\n"
        "add_imms r1, r1, 1\n"

        "restore_done:\n"

        /* Restore CFM configuration from stack */
        "popd cfm\n .cfi_def_cfa_offset 20\n .cfi_restore 11\n"

        /* Restore GPRs */
        "popd r3\n .cfi_def_cfa_offset 16\n .cfi_restore 3\n"
        "popd r2\n .cfi_def_cfa_offset 12\n .cfi_restore 2\n"
        "popd r1\n .cfi_def_cfa_offset 8\n .cfi_restore 1\n"
        "popd r0\n .cfi_def_cfa_offset 4\n .cfi_restore 0\n"

        /* Restore RA and return */
        "popd ra\n .cfi_def_cfa_offset 0\n .cfi_restore 15\n"
        "goto_reg ra\n"
    );
}
/* clang-format on */
#endif // __EZH_BITSLICE_INTERRUPTS__
