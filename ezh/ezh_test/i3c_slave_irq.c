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
 * INTERRUPT-DRIVEN I2C SLAVE on the EZH -- no polling.
 *
 * The bit-bang slave (ezh_i2c_slave_regmap.c) busy-polls the GPI lines. This
 * slave instead uses the on-chip I3C0 peripheral in I2C-legacy SLAVE mode
 * (static address 0x42, same J18 pins) and sleeps the EZH core in
 * __builtin_ezh_hold() between events. Each bus event is a real interrupt:
 *
 *   I3C0 SINTSET.{RXPEND,STOP} -> I3C0 IRQ line
 *     -> INPUTMUX SMART_DMA_TRIG_CH_SEL[0] = 26 (I3c0Irq) -> SmartDMA trig ch 0
 *       -> bitslice 0 (CFS = channel 0, CFM = level detect) -> combiner
 *         -> __builtin_ezh_hold() wakes  (the "ISR" is the code after the hold)
 *
 * Note the event fabric cannot see the pins themselves (slice inputs are
 * Port0/1 GPIO + IRQ lines only -- see EVENT_FABRIC.md), so an interrupt-driven
 * *bit-bang* slave is impossible on the J18 wiring; routing the peripheral's
 * IRQ is the correct idiom, and it is the same path the i3c_stream.c master
 * uses on the TX side. Together they form a transfer with no polling on either
 * side's data path.
 *
 * The IRQ is a level; a wake latches the slice's sticky flag, so after
 * servicing we re-write CFM to clear it (a CFM bit-op does NOT clear sticky
 * flags -- silicon fact). If a new byte raced in between drain and clear, the
 * still-high level re-latches immediately and the next hold falls through: no
 * event can be lost.
 *
 * Expects the transaction sent by i3c_stream.c: addr 0x42 write,
 * bytes [0x00, 0x10..0x1F] (17 bytes), then STOP. The M33 side (the runner)
 * configures SCONFIG/SINTSET/INPUTMUX before igniting this firmware.
 * Exits 0 -> exc_signal 0xCAFEBABE once all bytes and the STOP arrived.
 */

#include "ezh_test.h"

#define I3C0 0x40036000u
#define R(o) (*(volatile unsigned *)(I3C0 + (o)))
#define SSTATUS 0x08u
#define SRDATAB 0x40u

#define ST_STOP 0x400u    /* SSTATUS.STOP (W1C) */
#define ST_RXPEND 0x800u  /* SSTATUS.RX_PEND (clears when FIFO drained) */

#define NDATA 17u /* reg byte + 16 data bytes from i3c_stream.c */

/* Bitslice combiner encoding (see EVENT_FABRIC.md): 3 bits per slice at
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
#define BS_0 6u   /* force 0 (unused slices keep the combiner low) */
#define REST_FORCE0                                                            \
  (BS1(BS_0) | BS2(BS_0) | BS3(BS_0) | BS4(BS_0) | BS5(BS_0) | BS6(BS_0) |     \
   BS7(BS_0))

volatile unsigned s_holds = 0, s_stops = 0, s_rx = 0;
volatile unsigned char s_buf[24];

int main(void) {
  /* Arm bitslice 0 on trigger channel 0 (the routed I3C0 IRQ), level detect;
   * all other slices forced 0 so a hold genuinely blocks until the IRQ. */
  unsigned cfm = BS0(BS_SIG) | REST_FORCE0 | 0xFFu;
  __builtin_ezh_write_cfs(BS0(0u));
  __builtin_ezh_write_cfm(cfm);
  __builtin_ezh_write_cfm(__builtin_ezh_read_cfm()); /* clear stale flag */

  unsigned idx = 0u, holds = 0u, stops = 0u;
  for (;;) {
    __builtin_ezh_hold(); /* sleep until the I3C0 slave interrupt */
    holds++;

    unsigned st = R(SSTATUS);
    while (st & ST_RXPEND) { /* drain every byte this wake delivered */
      unsigned b = R(SRDATAB);
      if (idx < sizeof s_buf)
        s_buf[idx] = (unsigned char)b;
      idx++;
      st = R(SSTATUS);
    }
    if (st & ST_STOP) {
      R(SSTATUS) = ST_STOP; /* W1C */
      stops++;
    }

    __builtin_ezh_write_cfm(cfm); /* clear the sticky level flag */

    s_holds = holds; /* progress readable over JTAG while we run */
    s_rx = idx;
    s_stops = stops;

    if (idx >= NDATA && stops != 0u)
      break;
  }
  return 0; /* crt0 -> exc_signal = 0xCAFEBABE */
}
