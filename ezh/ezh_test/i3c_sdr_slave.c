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
 * TRUE I3C (SDR mode) SLAVE, fully interrupt-driven -- no polling.
 *
 * Same architecture as i3c_slave_irq.c (the EZH sleeps in
 * __builtin_ezh_hold(); the I3C0 IRQ routed INPUTMUX ch0 -> bitslice 0 wakes
 * it per event), but the bus now runs native I3C SDR and the slave takes part
 * in the I3C-specific protocol step: the master's SETDASA CCC assigns it a
 * dynamic address. The hardware handles the CCC itself; the firmware sees it
 * as a DACHG interrupt and records SDYNADDR (expect (0x30<<1)|DAVALID = 0x61)
 * -- proof the wake really was the dynamic-address assignment.
 *
 * Then the private SDR write (17 bytes, push-pull at ~4 MHz) arrives byte by
 * byte through RXPEND wakes, exactly as in the I2C demo. Note the CCC bytes
 * (0x87, the DA byte) never appear in the RX FIFO -- the CCC engine consumes
 * them -- so s_buf receives only the private-write payload.
 *
 * Exits 0 -> exc_signal 0xCAFEBABE once all bytes and a STOP arrived.
 */

#include "ezh_test.h"

#define I3C0 0x40036000u
#define R(o) (*(volatile unsigned *)(I3C0 + (o)))
#define SSTATUS 0x08u
#define SRDATAB 0x40u
#define SDYNADDR 0x64u

#define ST_STOP 0x400u   /* W1C */
#define ST_RXPEND 0x800u /* clears when FIFO drained */
#define ST_DACHG 0x2000u /* W1C: dynamic address changed (SETDASA) */

#define NDATA 17u /* payload of i3c_sdr_master.c */

/* Bitslice combiner encoding (see EVENT_FABRIC.md). */
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

volatile unsigned s_holds = 0, s_stops = 0, s_rx = 0, s_dachg = 0, s_dyn = 0;
volatile unsigned char s_buf[24];

int main(void) {
  unsigned cfm = BS0(BS_SIG) | REST_FORCE0 | 0xFFu;
  __builtin_ezh_write_cfs(BS0(0u));
  __builtin_ezh_write_cfm(cfm);
  __builtin_ezh_write_cfm(__builtin_ezh_read_cfm());

  unsigned idx = 0u, holds = 0u, stops = 0u, dachg = 0u;
  for (;;) {
    __builtin_ezh_hold(); /* sleep until the next I3C0 slave interrupt */
    holds++;

    unsigned st = R(SSTATUS);
    while (st & ST_RXPEND) {
      unsigned b = R(SRDATAB);
      if (idx < sizeof s_buf)
        s_buf[idx] = (unsigned char)b;
      idx++;
      st = R(SSTATUS);
    }
    if (st & ST_DACHG) { /* the SETDASA landed: capture our new identity */
      s_dyn = R(SDYNADDR);
      dachg++;
      R(SSTATUS) = ST_DACHG;
    }
    if (st & ST_STOP) {
      R(SSTATUS) = ST_STOP;
      stops++;
    }

    __builtin_ezh_write_cfm(cfm); /* clear the sticky level flag */

    s_holds = holds; /* JTAG-visible progress */
    s_rx = idx;
    s_stops = stops;
    s_dachg = dachg;

    if (idx >= NDATA && stops != 0u)
      break;
  }
  return 0; /* crt0 -> exc_signal = 0xCAFEBABE */
}
