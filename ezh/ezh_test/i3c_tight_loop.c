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
 * HW-I2C via SmartDMA -- stage M1b: the LITERAL tight_loop (OP_LOOP) hardware
 * loop feeding the HW I3C0 TX FIFO, written in pure C through the
 * __builtin_ezh_tight_loop compiler builtin. This is the first clean
 * end-to-end silicon validation of tight_loop (prior attempts never exercised
 * it in a working armed-event context; see ezh/EVENT_FABRIC.md).
 *
 * ---------------------------------------------------------------------------
 * HOW tight_loop WORKS (validated on EVK-MIMXRT595, board A):
 *
 *   tight_loop <Rend>, <Rcount>     ; both operands are REGISTERS
 *     <single straight-line body>   ; runs Rcount+1 times, no internal branches
 *   Rend:                           ; Rcount+1 iterations later, execution
 *                                   ; falls through to here (address in Rend)
 *
 *   - Rcount = (iteration count) - 1.  With Rcount=6 the body runs 7 times
 *     (confirmed: 7 bytes delivered byte-exact, no error).
 *   - Rend is a REGISTER holding the code address immediately AFTER the body.
 *     Materialising a code address in hand asm is the historical trap -- a
 *     hand-written `ldr rX, pc, <lit>` read back 0 at runtime. The compiler
 *     gets it right: `__builtin_ezh_tight_loop(&&loop_end, n-1)` turns the
 *     label-as-value into a blockaddress and emits a correct PC-relative
 *     literal-pool load. (Verified Rend = the instruction right after the
 *     body.) The builtin also register-allocates both operands, so no pinned
 *     `register ... asm("rN")` inline asm is needed at all.
 *   - The body must be a SINGLE basic block with no internal control flow.
 *   - Each iteration is meant to HOLD on a peripheral event (event-paced DMA);
 *     that is why NXP's own SmartDMA firmware bodies always contain a hold.
 *   - BODY CONTRACT for the builtin (see IntrinsicsEZH.td): the compiler
 *     models the body as straight-line code that runs once, so every body
 *     operation must be side-effecting (EZH intrinsics / volatile accesses)
 *     and loop-carried state must live in volatile storage -- otherwise the
 *     optimizer will legitimately fold it (e.g. delete a dead `ptr++`).
 *
 * WHY THE STREAM HERE FITS THE FIFO (NDATA small):
 *   The hold INSIDE tight_loop does NOT cleanly pace a *level* peripheral event
 *   the way a plain hold in a C loop does: the 2-slot scheduled loop-back's
 *   delay slots + the level TXNOTFULL signal + the per-iteration CFM flag-clear
 *   do not compose into one-write-per-drained-slot. Streaming >8 bytes with
 *   tight_loop overflows the FIFO (OWRITE; only the first 8 land). So this test
 *   streams <= 7 bytes, which fit the 8-deep FIFO with no pacing required, and
 *   thereby validates tight_loop's COUNT/ITERATION/Rend semantics unambiguously.
 *   For real >FIFO streaming use the plain C loop + __builtin_ezh_hold in
 *   i3c_stream.c -- it paces a level event robustly and is what belongs in
 *   compiled code. tight_loop stays a hand-written primitive for MHz-class
 *   event streams (e.g. FLEXIO display DMA) where the loop-branch overhead the
 *   OP_LOOP removes is actually measurable.
 * ---------------------------------------------------------------------------
 *
 * Sends: addr 0x42, write [reg=0][0x10..0x16]  ->  slave regmap[0..6]=0x10..0x16
 */

#include "ezh_test.h"

#define I3C0 0x40036000u
#define R(o) (*(volatile unsigned *)(I3C0 + (o)))
#define MSTATUS 0x88u
#define MWDATABE 0xB4u

#define BS0(c) ((c) << 8)
#define BS1(c) ((c) << 11)
#define BS2(c) ((c) << 14)
#define BS3(c) ((c) << 17)
#define BS4(c) ((c) << 20)
#define BS5(c) ((c) << 23)
#define BS6(c) ((c) << 26)
#define BS7(c) ((c) << 29)
#define BS_SIG 4u
#define BS_0 6u
#define REST_FORCE0                                                            \
  (BS1(BS_0) | BS2(BS_0) | BS3(BS_0) | BS4(BS_0) | BS5(BS_0) | BS6(BS_0) |     \
   BS7(BS_0))

/* reg index (0) followed by the data bytes streamed BY THE LOOP. The final data
 * byte (0x16) is sent after the loop via MWDATABE so the master appends a STOP.
 * reg + (this many) + 1 = 8 bytes total, exactly the FIFO depth. */
static const unsigned char data[7] = {0x00, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15};

volatile unsigned m_mctrldone = 0, m_complete = 0, m_rend = 0;

int main(void) {
  unsigned cfm = BS0(BS_SIG) | REST_FORCE0 | 0xFFu;
  __builtin_ezh_write_cfs(BS0(0u));
  __builtin_ezh_write_cfm(cfm);
  __builtin_ezh_write_cfm(__builtin_ezh_read_cfm());

  R(MSTATUS) = 0xFFFFFFFFu;
  R(0x84u) = 1u | (1u << 4) | (0x42u << 9); /* START + addr 0x42 write */
  unsigned to = 6000000u;
  while (!(R(MSTATUS) & 0x200u) && --to) {
  }
  m_mctrldone = (to != 0u);
  R(MSTATUS) = 0x200u;

  /* --- the literal tight_loop hardware loop, via the compiler builtin ---
   *
   * __builtin_ezh_tight_loop(rend, rcount) emits the raw OP_LOOP instruction;
   * the compiler materialises Rend (the `&&loop_end` blockaddress becomes a
   * PC-relative literal-pool load) and Rcount, and register-allocates both --
   * no pinned-register inline asm needed.
   *
   * Body contract (the compiler models the body as straight-line code that
   * runs ONCE, so anything it may legally fold away, it will): every operation
   * must be side-effecting -- EZH intrinsics and volatile accesses -- and any
   * loop-carried state must live in volatile storage. Here the advancing data
   * pointer is a volatile object: its load and store are ordered side effects,
   * so the increment survives and re-executes on every hardware iteration. */
  const unsigned char *volatile vptr = data;
  m_rend = (unsigned)&&loop_end;
  __builtin_ezh_tight_loop(&&loop_end, 7u - 1u); /* Rcount = iterations - 1 */
  /* ---- body: runs 7 times, one straight-line block ---- */
  __builtin_ezh_hold();          /* pace on the I3C0 TX event  */
  {
    const unsigned char *p = vptr;
    R(0xB0u) = *p;               /* MWDATAB = *p               */
    vptr = p + 1;                /* advance (volatile store)   */
  }
  __builtin_ezh_write_cfm(cfm);  /* clear the sticky slice flag */
  /* ---- end of body ---- */
loop_end:
  __builtin_ezh_hold();
  R(MWDATABE) = 0x16u; /* last data byte -> STOP */

  to = 6000000u;
  while (!(R(MSTATUS) & 0x400u) && --to) {
  }
  m_complete = (to != 0u);
  return 0;
}
