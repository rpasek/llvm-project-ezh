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
 * Read-probe slave: the target half of the SDR-read timing experiment.
 * After ENTDAA gives it a dynamic address, it does nothing but keep its
 * TX FIFO topped up with a rolling byte pattern so the master's repeated
 * reads always have data to clock. It never sleeps -- a tight feed loop
 * keeps the FIFO full regardless of when the master reads. Byte values do
 * not matter to the experiment (which measures RXCOUNT-vs-COMPLETE timing
 * on the master), only that N bytes actually transfer each read.
 */

#define I3C0 0x40036000u
#define R(o) (*(volatile unsigned *)(I3C0 + (o)))
#define SSTATUS 0x08u
#define SDATACTRL 0x2Cu
#define SWDATAB 0x30u
#define SRDATAB 0x40u
#define SDYNADDR 0x64u

#define ST_RXPEND 0x800u
#define ST_DACHG 0x2000u   /* W1C: dynamic address changed */
#define ST_STOP 0x400u
#define TX_FULL 0x40000000u

volatile unsigned s_dyn = 0, s_fed = 0, s_stops = 0;

int main(void) {
  /* Wait (polling, no event fabric needed here) for ENTDAA to assign us a
   * dynamic address. The hardware handles the ENTDAA arbitration; we just
   * watch for the DACHG flag. */
  unsigned fed = 0u, stops = 0u;
  for (unsigned guard = 0u; guard < 20000000u; guard++) {
    unsigned st = R(SSTATUS);
    while (st & ST_RXPEND) { /* drain any stray CCC bytes */
      (void)R(SRDATAB);
      st = R(SSTATUS);
    }
    if (st & ST_DACHG) {
      s_dyn = R(SDYNADDR);
      R(SSTATUS) = ST_DACHG;
      break;
    }
  }

  /* Serve reads: keep the TX FIFO full with a rolling pattern, forever
   * (bounded by a large guard so it exits cleanly). */
  for (unsigned guard = 0u; guard < 200000000u; guard++) {
    if (!(R(SDATACTRL) & TX_FULL)) {
      R(SWDATAB) = (fed & 0xFFu);
      fed++;
      if ((fed & 0x3FFu) == 0u) {
        s_fed = fed;            /* JTAG-visible progress */
        s_dyn = R(SDYNADDR);    /* refresh the assigned dynamic address */
      }
    }
    unsigned st = R(SSTATUS);
    if (st & ST_RXPEND)
      (void)R(SRDATAB);
    if (st & ST_STOP) {
      R(SSTATUS) = ST_STOP;
      stops++;
      s_stops = stops;
    }
  }
  s_fed = fed;
  s_dyn = R(SDYNADDR); /* final address readback regardless of DACHG timing */
  return 0;
}
