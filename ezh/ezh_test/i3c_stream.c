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
 * HW-I2C via SmartDMA -- stage M1: EVENT-PACED streaming to the HW I3C0 FIFO.
 *
 * We stream 16 data bytes -- twice the 8-deep TX FIFO -- so a blind push would
 * overflow. Instead each byte write is gated by __builtin_ezh_hold(), which
 * blocks the core on the I3C0 "TX FIFO not full" event and wakes when the master
 * has clocked a byte out and freed a slot. The core is idle between drains; the
 * hardware peripheral paces the CPU, which is the whole point of the SmartDMA.
 *
 * Event path (armed by the M33 in run_i3c_smartdma.sh):
 *   I3C0 MINTSET.TXNOTFULL -> IRQ line
 *     -> INPUTMUX SMART_DMA_TRIG_CH_SEL[0] = 26 (I3c0Irq) -> SmartDMA trig ch 0
 *       -> bitslice 0 (CFS = channel 0, CFM = level detect) -> logical combiner
 *         -> __builtin_ezh_hold() wakes
 *
 * The TXNOTFULL signal is a LEVEL (asserted while any slot is free). A hold in
 * level mode latches a sticky bitslice flag, so after writing a byte we re-write
 * CFM to clear it -- otherwise the stale "was ready" level double-wakes the next
 * hold and we write two bytes into one freed slot (overflow: the first 8 bytes
 * land, then every other byte drops). Clearing the flag per byte = exactly one
 * write per drained slot.
 *
 * Sends: addr 0x42, write [reg=0][0x10..0x1F]  ->  slave regmap[0..15]=0x10..0x1F
 */

#include "ezh_test.h"

#define I3C0 0x40036000u
#define R(o) (*(volatile unsigned *)(I3C0 + (o)))
#define MCTRL 0x84u
#define MSTATUS 0x88u
#define MWDATAB 0xB0u
#define MWDATABE 0xB4u

/* Bitslice combiner encoding (see ezh.h / EVENT_FABRIC.md): 3 bits per slice at
 * bit 8+3n select the detect mode; the low byte is the per-slice OR-enable. */
#define BS0(c) ((c) << 8)
#define BS1(c) ((c) << 11)
#define BS2(c) ((c) << 14)
#define BS3(c) ((c) << 17)
#define BS4(c) ((c) << 20)
#define BS5(c) ((c) << 23)
#define BS6(c) ((c) << 26)
#define BS7(c) ((c) << 29)
#define BS_SIG 4u /* level-high detect */
#define BS_0 6u   /* force 0 (so unused slices keep the combiner low) */
#define REST_FORCE0                                                            \
  (BS1(BS_0) | BS2(BS_0) | BS3(BS_0) | BS4(BS_0) | BS5(BS_0) | BS6(BS_0) |     \
   BS7(BS_0))

volatile unsigned m_mctrldone = 0, m_complete = 0, m_holds = 0;

int main(void) {
  /* Arm bitslice 0 on SmartDMA trigger channel 0 (the routed I3C0 TX IRQ),
   * level detect; all other slices forced 0 so the combiner is 0 until slice 0
   * goes high -- i.e. a hold actually blocks. */
  unsigned cfm = BS0(BS_SIG) | REST_FORCE0 | 0xFFu;
  __builtin_ezh_write_cfs(BS0(0u));
  __builtin_ezh_write_cfm(cfm);
  __builtin_ezh_write_cfm(__builtin_ezh_read_cfm()); /* clear stale flag */

  R(MSTATUS) = 0xFFFFFFFFu;
  R(MCTRL) = 1u | (1u << 4) | (0x42u << 9); /* START + addr 0x42 write */
  unsigned to = 6000000u;
  while (!(R(MSTATUS) & 0x200u) && --to) {
  }
  m_mctrldone = (to != 0u);
  R(MSTATUS) = 0x200u;

  /* reg = 0, then 16 data bytes 0x10..0x1F, one per drained FIFO slot. */
  unsigned holds = 0u;
  __builtin_ezh_hold();
  R(MWDATAB) = 0x00u; /* reg = 0 */
  __builtin_ezh_write_cfm(cfm);
  holds++;
  for (unsigned i = 0u; i < 15u; i++) {
    __builtin_ezh_hold();  /* block until a slot frees */
    R(MWDATAB) = 0x10u + i;
    __builtin_ezh_write_cfm(cfm); /* clear the sticky level flag */
    holds++;
  }
  __builtin_ezh_hold();
  R(MWDATABE) = 0x1Fu; /* last byte -> STOP */
  holds++;
  m_holds = holds;

  to = 6000000u;
  while (!(R(MSTATUS) & 0x400u) && --to) {
  }
  m_complete = (to != 0u);
  return 0;
}
