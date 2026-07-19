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
 * HW-I2C via SmartDMA -- stage M0b: the EZH drives the on-chip I3C0 peripheral
 * in legacy-I2C master mode by poking its registers with per_write, sending a
 * short register-map write to the board-B bit-bang slave over the wired I3C
 * header (PIO2_29=SCL, PIO2_30=SDA). No bit-banging: I3C0 generates the START,
 * address, ACK sampling, byte timing and STOP; the EZH is its data engine.
 *
 * The M33 does the one-time peripheral bring-up (clock tree, the mandatory
 * peripheral-reset pulse, MCONFIG, pin mux) and leaves I3C0 at STATE=IDLE; see
 * run_i3c_smartdma.sh. This firmware just runs the transaction.
 *
 * Sends: addr 0x42, write [reg=8][0x11][0x22][0x33][0x44]
 * Expect on the slave: regmap[8..11] = {0x11,0x22,0x33,0x44}  (= 0x44332211 LE)
 */

#include "ezh_test.h"

#define I3C0 0x40036000u
#define R(o) (*(volatile unsigned *)(I3C0 + (o)))
/* I3C master registers (offsets from I3C0 base). */
#define MCTRL 0x84u    /* REQUEST[2:0] | TYPE[5:4] | DIR[8] | ADDR[15:9] */
#define MSTATUS 0x88u  /* STATE[2:0] MCTRLDONE(9) COMPLETE(10) TXNOTFULL(12) */
#define MWDATAB 0xB0u  /* write a non-last data byte into the TX FIFO */
#define MWDATABE 0xB4u /* write the LAST data byte (I3C0 appends the STOP) */

volatile unsigned m_mctrldone = 0, m_complete = 0, m_status = 0;

int main(void) {
  /* MSTATUS is W1C for the event bits; clear stale flags before the request. */
  R(MSTATUS) = 0xFFFFFFFFu;
  /* START + address 0x42, direction = write, bus type = legacy I2C. */
  R(MCTRL) = 1u /*EmitStartAddr*/ | (1u << 4) /*TYPE=I2C*/ | (0x42u << 9);
  unsigned to = 6000000u;
  while (!(R(MSTATUS) & 0x200u) && --to) { /* wait MCTRLDONE (addr sent+ACKed) */
  }
  m_mctrldone = (to != 0u);
  R(MSTATUS) = 0x200u; /* clear MCTRLDONE */

  /* Payload: reg index then four data bytes. Five bytes fit the 8-deep FIFO,
   * so a blind push is safe here (see i3c_stream.c for >FIFO event pacing). */
  R(MWDATAB) = 0x08u;  /* reg = 8 */
  R(MWDATAB) = 0x11u;
  R(MWDATAB) = 0x22u;
  R(MWDATAB) = 0x33u;
  R(MWDATABE) = 0x44u; /* last byte -> STOP */

  to = 6000000u;
  while (!(R(MSTATUS) & 0x400u) && --to) { /* wait COMPLETE */
  }
  m_complete = (to != 0u);
  m_status = R(MSTATUS);
  return 0;
}
