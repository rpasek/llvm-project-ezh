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
 * Real-edge event-fabric harness (STAGE 1: plain hold).
 *
 * The M33 (over OpenOCD) routes a GPIO pin -> SmartDMA trigger channel 0 via
 * INPUTMUX, configures the pin as an output, ignites this firmware, waits for
 * the holding marker, then drives a rising edge on the pin. This firmware arms
 * bit-slice 0 on channel 0 (rising), publishes a holding marker, and e_holds.
 * If the real edge wakes it, exc_signal becomes 0xCAFEBABE -- proving the
 * INPUTMUX -> channel -> CFS/CFM -> slice -> combiner path works with a genuine
 * pin edge (which the PENDTRAP software doorbell could not drive for dispatch).
 *
 * #ifdef VECTORED: STAGE 2 -- e_vectored_hold over a measurement table.
 * RESULT (definitive, measured on silicon w/ an explicit self-spin table +
 * inline marker): in this JTAG-ignited harness e_vectored_hold does NOT
 * table-dispatch -- it WAKES on the real edge and RESUMES INLINE (resuming ~2
 * instructions past the hold, the scheduled shadow slots / via RA), exactly
 * like a plain hold; PC never enters the table. The bit-slice combiner supplies
 * the WAKE but no per-slice vector index, so the dispatch path isn't taken.
 * Firing the real table-dispatch needs the vector-index state the full M33
 * fsl_smartdma boot arms (see ../m33_vhold_harness/), not the minimal ignite.
 */

#include "ezh.h"
#include "ezh_test.h"

#define HOLDING_MARKER 0xB0B0B0B0u

#ifdef VECTORED
extern char vh_table[];
/* Table reached only by the vectored-hold dispatch. r4 (= &exc_signal) and r5
 * (= 0, the counter) are materialised by the compiler before the hold and
 * survive it -- NOT via "e_load_imm rX, symbol", which does not load a 32-bit
 * address (it drops the address inline as a bogus word). */
__asm__(".pushsection .text\n"
        ".global vh_table\n"
        "vh_table:\n"
        ".rept 256\n"
        "  e_add_imm r5, r5, 1\n"
        ".endr\n"
        "  e_str r5, r4, 0\n" /* *(uint32*)r4 = r5 = 256 - landing_slot */
        "vh_spin: e_gotol vh_spin\n"
        ".popsection\n");
#endif

int main(void) {
  exc_signal = 0x11111111;

  /* Arm bit-slice 0: source = SmartDMA trigger channel 0, rising-edge detect,
   * OR-enable bit 0 into the combiner. (CFM write also clears a stale BS flag.) */
  ezh_write_cfs(BS0(EZH_INPUT_SOURCE_0));
  /* Slice 0 = active detect on channel 0; slices 1-7 = BS_0 (force-0); OR-enable
   * all 8 (0xFF), then clear BS. This makes the combiner 0 (so e_hold/vectored
   * hold actually BLOCK) until slice 0 fires -- the key the earlier attempts
   * missed (CFM=0 leaves the combiner at 1). */
#define REST_FORCE0                                                            \
  (BS1(BS_0) | BS2(BS_0) | BS3(BS_0) | BS4(BS_0) | BS5(BS_0) | BS6(BS_0) |     \
   BS7(BS_0))
#ifdef LEVEL
  ezh_write_cfm(BS0(BS_SIG) | REST_FORCE0 | 0xFFu); /* level-high detect */
#else
  ezh_write_cfm(BS0(BS_RISE) | REST_FORCE0 | 0xFFu); /* rising-edge detect */
#endif
  ezh_write_cfm(ezh_read_cfm()); /* clear stale BS */

  exc_signal = (int)HOLDING_MARKER; /* tell the M33 we are about to hold */

#ifdef VECTORED
  /* Pass the table base and &exc_signal as operands so the compiler loads the
   * full 32-bit addresses (PC-relative literal pool); r4/r5 survive the hold. */
  __asm__ volatile("e_mov r4, %1\n\t"          /* r4 = &exc_signal           */
                   "e_load_imm r5, 0\n\t"      /* r5 = 0 (counter)           */
                   "e_vectored_hold %0\n\t"    /* table base = vh_table       */
                   :
                   : "r"(vh_table), "r"((void *)&exc_signal)
                   : "r4", "r5", "memory");
  /* vh_table's epilogue writes exc_signal (= 64 - landing_slot) and spins. */
  for (;;)
    ;
#else
  __builtin_ezh_hold(); /* wakes on the real rising edge into slice 0 */
  exc_signal = (int)0xCAFEBABE;
  return (int)0xCAFEBABE;
#endif
}
