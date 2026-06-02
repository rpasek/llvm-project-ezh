//===-- int_div_impl.inc - EZH Integer division/modulo helper -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#if defined(INT_DIV)
#define FUNCTION_NAME __udivsi3
#define RETURN_QUOTIENT
#elif defined(INT_MOD)
#define FUNCTION_NAME __umodsi3
#define RETURN_REMAINDER
#else
#error "Either INT_DIV or INT_MOD must be defined before including this file"
#endif

/* clang-format off */
.text
.p2align 2

.globl FUNCTION_NAME
.type FUNCTION_NAME, @function
.hidden FUNCTION_NAME

FUNCTION_NAME:
    ; 1. Save callee-saved registers to stack
    pushd r4
    pushd r5

    ; 2. Setup loop parameters
    load_imm r5, 31             ; Loop count = 31 (runs 32 times total)
    load_imm r2, 0              ; R2 = Remainder (initialized to 0)

    ; 3. Setup hardware zero-overhead tight loop safely
    ldr r4, pc, 1f              ; SAFE: Load Rend from constant island
    tight_loop r4, r5           ; Enable tight loop (executes R5+1 times)
    nop                         ; Quirk: instruction after tight_loop only
                                ; executes ONCE at entry. NOP absorbs cycle.

    ; --- Loop Body (32 iterations, 6 cycles, no branching overhead) ---
    lsls r0, r0, 1              ; 1. Shift R0 left by 1, MSB -> Carry flag
    lsl r2, r2, 1               ; 2. Shift R2 left by 1 (room for MSB)
    bset_imm_ca r2, r2, 0       ; 3. Shift Carry into LSB of remainder R2
    subs r3, r2, r1             ; 4. R3 = R2 - R1, Carry = unsigned borrow
                                ;    (No Borrow -> R2 >= R1 -> Carry == 0)
    mov_nc r2, r3               ; 5. Conditional Restore: R2 = R3 if NC == 1
    bset_imm_nc r0, r0, 0       ; 6. Set Quotient Bit: R0[0] = 1 if NC == 1
    ; --- Loop Ends Here ---

loop_end_label:
#if defined(RETURN_REMAINDER)
    mov r0, r2                  ; Modulo: Return remainder in R0
#endif
    ; 4. Restore registers and return
    popd r5
    popd r4
    goto_reg ra

.p2align 2
1:  .long loop_end_label

.size FUNCTION_NAME, . - FUNCTION_NAME
/* clang-format on */
