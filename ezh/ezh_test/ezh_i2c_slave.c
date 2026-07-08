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
 * Bit-banged I2C slave on the EZH/SmartDMA core, compiled by ezh-none-elf (no
 * hand assembly). Lines are the I3C0 bus on header J18 of the EVK-MIMXRT595:
 *   SDA = PIO2_30 = SmartDMA_PIO30 = GPI/GPD bit 30
 *   SCL = PIO2_29 = SmartDMA_PIO29 = GPI/GPD bit 29
 * Open-drain: read levels with read_gpi(); pull a line low with
 * gpd_drive_low(bit); release (float high via the external pull-up) with
 * gpd_release(bit).
 *
 * Services ONE write/read transaction to address 0x42 then returns (so it
 * doubles as a harness test, exc_signal == 0xCAFEBABE on success). i2c_rx_byte
 * holds whatever the master wrote; the runner verifies it.
 *
 * SPEED: the inner SCL-edge polls are the bottleneck for a polled slave, so
 * they are kept to the bare minimum -- read GPI, test the SCL bit, branch --
 * with NO per-edge timeout. SDA is sampled from the SAME GPI word that catches
 * the SCL rising edge (no extra read). Only the wait-for-a-master START search
 * is bounded; once a START is seen the bit loops run untimed (a torn-off
 * transaction is recovered by the next ignite). This carries the bus well past
 * I2C fast-mode (see run_i2c_speed.sh).
 */

#include <stdint.h>

#define SDA_BIT 30u            /* PIO2_30 / I3C0_SDA / J18 */
#define SCL_BIT 29u            /* PIO2_29 / I3C0_SCL / J18 */
#define SDA_MASK (1u << SDA_BIT)
#define SCL_MASK (1u << SCL_BIT)

#define SLAVE_ADDR 0x42u
#define TX_BYTE 0x5Au          /* what the read path returns to a master read */

#define START_TIMEOUT 100000000u /* wait (essentially) until a master appears;
                                  * returns early the instant a START arrives */

/* IOPCTL pin-mux (i.MX RT500): PIO[port][pin] = 0x40004000 + port*0x80 + pin*4.
 * SmartDMA is Func 15; IBENA lets read_gpi see the pad, ODENA = open-drain. */
#define IOPCTL_PIO2_29 (*(volatile uint32_t *)0x40004174u)
#define IOPCTL_PIO2_30 (*(volatile uint32_t *)0x40004178u)
#define PIN_SMARTDMA_I2C                                                        \
  (0xFu /*FSEL=15*/ | 0x10u /*PUPDENA*/ | 0x20u /*PUPDSEL=up*/ |                \
   0x40u /*IBENA*/ | 0x400u /*ODENA*/)

/* Mirror of the result for inspection over the debugger. */
volatile uint32_t i2c_status = 0u;
volatile uint32_t i2c_rx_byte = 0u;

/* Receive one byte, MSB first. The wait-for-SCL-high read IS the data sample,
 * so each bit costs only the two edge polls -- no separate SDA read. */
static unsigned recv_byte(void) {
  unsigned byte = 0u, io;
  for (unsigned b = 0u; b < 8u; b++) {
    while (__builtin_ezh_read_gpi() & SCL_MASK) /* wait for SCL low  */
      continue;
    do {
      io = __builtin_ezh_read_gpi();            /* wait for SCL high + sample SDA */
    } while (!(io & SCL_MASK));
    byte = (byte << 1) | ((io >> SDA_BIT) & 1u);
  }
  return byte;
}

/* Pull SDA low across one SCL high pulse (ACK), then release it. */
static void ack(void) {
  while (__builtin_ezh_read_gpi() & SCL_MASK) /* SCL low */
    continue;
  __builtin_ezh_gpd_drive_low(SDA_BIT);
  while (!(__builtin_ezh_read_gpi() & SCL_MASK)) /* SCL high: master samples ACK */
    continue;
  while (__builtin_ezh_read_gpi() & SCL_MASK) /* SCL low */
    continue;
  __builtin_ezh_gpd_release(SDA_BIT);
}

int main(void) {
  /* Route PIO2_29/30 to the SmartDMA function with input + open-drain. */
  IOPCTL_PIO2_29 = PIN_SMARTDMA_I2C;
  IOPCTL_PIO2_30 = PIN_SMARTDMA_I2C;
  __builtin_ezh_gpd_release(SDA_BIT);
  __builtin_ezh_gpd_release(SCL_BIT);

  /* 1. Detect START: SDA low while SCL high (state-based; the idle bus is high
   * so the first such sample is the real START). */
  unsigned io = 0u, found = 0u;
  for (unsigned t = 0u; t < START_TIMEOUT; t++) {
    io = __builtin_ezh_read_gpi();
    if (!(io & SDA_MASK) && (io & SCL_MASK)) {
      found = 1u;
      break;
    }
  }
  if (!found) {
    /* No START: report the bus level (0xB0500011 = idle high, pins OK). */
    i2c_status = 0x71u;
    return (int)(0xB0500000u | ((io & SDA_MASK) ? 0x10u : 0u) |
                 ((io & SCL_MASK) ? 1u : 0u));
  }

  /* 2. Address byte (7-bit addr + R/W). */
  unsigned byte = recv_byte();
  unsigned addr = byte >> 1;
  unsigned rw = byte & 1u;
  if (addr != SLAVE_ADDR) {
    i2c_status = 0x74u;
    return (int)0xBAD00074u; /* not addressed (implicit NACK) */
  }

  /* 3. ACK the address. */
  ack();

  if (rw == 0u) {
    /* Master write: receive one data byte and ACK it. */
    unsigned data = recv_byte();
    ack();
    i2c_rx_byte = data; /* whatever the master sent -- verified by the runner */
    i2c_status = 0x52u; /* 'R' */
    return (int)0xCAFEBABEu;
  }

  /* Master read: shift out TX_BYTE, MSB first, set up while SCL is low. */
  unsigned tx = TX_BYTE;
  for (unsigned b = 0u; b < 8u; b++) {
    while (__builtin_ezh_read_gpi() & SCL_MASK) /* SCL low */
      continue;
    if (tx & 0x80u)
      __builtin_ezh_gpd_release(SDA_BIT);
    else
      __builtin_ezh_gpd_drive_low(SDA_BIT);
    tx = (tx << 1) & 0xFFu;
    while (!(__builtin_ezh_read_gpi() & SCL_MASK)) /* SCL high: master samples */
      continue;
  }
  while (__builtin_ezh_read_gpi() & SCL_MASK) /* SCL low */
    continue;
  __builtin_ezh_gpd_release(SDA_BIT); /* release for the master's ACK/NACK */
  while (!(__builtin_ezh_read_gpi() & SCL_MASK))
    continue;
  i2c_status = 0x54u; /* 'T' */
  return (int)0xCAFEBABEu;
}
