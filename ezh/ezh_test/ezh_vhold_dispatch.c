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

/*
 * vectored_hold dispatch — SOLVED and silicon-proven (EVK-MIMXRT595).
 *
 * The instruction's true contract (RE'd via a Dest/table field split + a
 * doorbell-driven discrimination matrix; see EVENT_FABRIC.md):
 *
 *   acc_vectored_hold  <encoding fields>:
 *     bits[13:10] = rDest   register RECEIVING the vector (PC => auto-GOTO)
 *     bits[23:20] = rTable  jump-table base register
 *     bits[31:24] = mask    per-slice DISPATCH-ENABLE mask ("vectors")
 *     bit9        = 1       (the ACC form)
 *
 *   Blocks like a plain hold until the bitslice combiner wakes it. On a wake
 *   won by slice n:
 *     mask bit n set:   rDest := rTable + 4 + 4*n   (AN14650's base+4+slice)
 *                       -- with rDest = PC that IS the hardware dispatch.
 *     mask bit n clear: resume inline (the spurious-wake path).
 *
 *   The PLAIN vectored_hold encodes mask = 0, so it can NEVER dispatch --
 *   which is why every earlier attempt (and NXP's own E_VECTORED_HOLD macro,
 *   which never appears in production firmware) resumes inline. The
 *   dispatch-arming is entirely inside the instruction word.
 *
 * This test: slices 0 and 2 armed (BS_EVENT detect, sources = SmartDMA trigger
 * channels 0/2), held via the compiler builtin __builtin_ezh_acc_vectored_hold
 * (mask = 0xFF), dispatched through the returned vector with a mov pc.
 * The runner (ezh_vhold_dispatch.py) parks the core in the hold, then fires
 * the channel-0 or channel-2 software doorbell (PENDTRAP EN(n)|REQ(n) at
 * 0x40027048); the hardware GOTOs vh_table + 4 + 4*slice with NO jump
 * instruction in the firmware. A 256-slot counting table reveals the landing
 * slot: land = 256-K  =>  K = 1 for slice 0, K = 3 for slice 2 (both observed
 * byte-exact on silicon).
 */

#include "ezh_test.h"

#define BS0(c) ((c) << 8)
#define BS1(c) ((c) << 11)
#define BS2(c) ((c) << 14)
#define BS3(c) ((c) << 17)
#define BS4(c) ((c) << 20)
#define BS5(c) ((c) << 23)
#define BS6(c) ((c) << 26)
#define BS7(c) ((c) << 29)
#define BS_EVENT 7u /* non-sticky event detect */
#define BS_0 6u     /* force-0 (unused slices keep the combiner low) */
#define HOLDING 0xB0B0B0B0u

volatile unsigned land = 0, vec_val = 0;
extern char vh_table[];

/* 256 counting slots: a dispatch landing at slot K executes 256-K add_imms,
 * then the epilogue stores the count and self-spins. */
__asm__(".pushsection .text\n"
        ".global vh_table\n"
        ".p2align 2\n"
        "vh_table:\n"
        ".rept 256\n"
        "  add_imm r5, r5, 1\n"
        ".endr\n"
        "  str r4, r5, 0\n" /* land = r5 = 256 - K */
        "vhspin: goto vhspin\n"
        ".popsection\n");

int main(void) {
  /* slice0 <- trig ch0, slice2 <- trig ch2, BS_EVENT detect, rest forced 0 */
  __builtin_ezh_write_cfs(BS0(0u) | BS2(2u));
  __builtin_ezh_write_cfm(BS0(BS_EVENT) | BS1(BS_0) | BS2(BS_EVENT) |
                          BS3(BS_0) | BS4(BS_0) | BS5(BS_0) | BS6(BS_0) |
                          BS7(BS_0) | 0xFFu);
  __builtin_ezh_write_cfm(__builtin_ezh_read_cfm()); /* clear stale flags */

  exc_signal = (int)HOLDING; /* tell the M33 we are about to hold */

  /* The compiler surface: __builtin_ezh_acc_vectored_hold blocks until a
   * bitslice event and returns the hardware-computed vector address
   * (vh_table + 4 + 4*slice) for a mask-enabled winning slice. The caller
   * dispatches through the result -- note this must be a `mov pc` in asm, NOT
   * a C computed goto: `goto *vec` lets the optimizer assume the target is an
   * in-function address-taken label and delete the indirection entirely. */
  void *vec = __builtin_ezh_acc_vectored_hold((void *)vh_table, 0xFFu);
  vec_val = (unsigned)vec; /* cross-check: == vh_table + 4 + 4*slice */
  __asm__ volatile("mov r4, %0\n\t"     /* r4 = &land (table epilogue) */
                   "load_imm r5, 0\n\t" /* r5 = slot counter           */
                   "mov pc, %1\n\t"     /* DISPATCH through the vector */
                   :
                   : "r"((void *)&land), "r"(vec)
                   : "r4", "r5", "memory");
  exc_signal = 0x001ED1ED; /* reached ONLY on a masked-out (spurious) wake */
  for (;;)
    ;
}
