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
 * Bit-banged I2C MASTER exercising the register-mapped slave
 * (ezh_i2c_slave_regmap.c) across all three address forms it now decodes:
 *   1. 7-bit  : write blocks to regs 8 and 2, read them back, self-verify;
 *   2. general call (0x00) : broadcast-write a block, read it back via 7-bit;
 *                            then a general-call reset (0x00,0x06);
 *   3. 10-bit (0x242)      : write a block via the 11110xx frame, read it back
 *                            via the combined-format (write addr, Sr, read).
 * Runs on the second EVK-MIMXRT595, wired J18<->J18. Returns 0xCAFEBABE iff
 * every byte ACKed and every read-back matched; m_status is a failure bitmask.
 */

#include <stdint.h>

#define SDA_BIT 30u
#define SCL_BIT 29u

#define SLAVE_ADDR 0x42u
#define ADDR10 0x242u
#define ADDR10_HI ((ADDR10 >> 8) & 0x3u)
#define ADDR10_LO (ADDR10 & 0xFFu)
#define A10_W (0xF0u | (ADDR10_HI << 1) | 0u) /* 11110 XX 0 */
#define A10_R (0xF0u | (ADDR10_HI << 1) | 1u) /* 11110 XX 1 */
#define GEN_CALL 0x00u

#ifndef HALF
#define HALF 100u
#endif

#define IOPCTL_PIO2_29 (*(volatile uint32_t *)0x40004174u)
#define IOPCTL_PIO2_30 (*(volatile uint32_t *)0x40004178u)
#define PIN_SMARTDMA_I2C (0xFu | 0x10u | 0x20u | 0x40u | 0x400u)

volatile uint32_t m_status = 0u;
volatile uint32_t m_rb_7bit = 0u;
volatile uint32_t m_rb_gc = 0u;
volatile uint32_t m_rb_10bit = 0u;

static inline unsigned sda_lvl(unsigned io) { return (io >> SDA_BIT) & 1u; }
static void dly(unsigned n) {
  while (n--)
    __asm__ volatile("");
}
static void scl_high(void) { __builtin_ezh_gpd_release(SCL_BIT); }
static void scl_low(void) { __builtin_ezh_gpd_drive_low(SCL_BIT); }
static void sda_high(void) { __builtin_ezh_gpd_release(SDA_BIT); }
static void sda_low(void) { __builtin_ezh_gpd_drive_low(SDA_BIT); }

static void bus_start(void) {
  sda_high();
  scl_high();
  dly(HALF);
  sda_low();
  dly(HALF);
  scl_low();
  dly(HALF);
}
static void bus_restart(void) {
  sda_high();
  dly(HALF);
  scl_high();
  dly(HALF);
  sda_low();
  dly(HALF);
  scl_low();
  dly(HALF);
}
static void bus_stop(void) {
  sda_low();
  dly(HALF);
  scl_high();
  dly(HALF);
  sda_high();
  dly(HALF);
}

static unsigned tx_byte(unsigned b) {
  for (unsigned i = 0u; i < 8u; i++) {
    if (b & 0x80u)
      sda_high();
    else
      sda_low();
    b = (b << 1) & 0xFFu;
    dly(HALF);
    scl_high();
    dly(HALF);
    scl_low();
  }
  sda_high();
  dly(HALF);
  scl_high();
  dly(HALF / 2u);
  unsigned ack = sda_lvl(__builtin_ezh_read_gpi());
  dly(HALF / 2u);
  scl_low();
  return ack;
}

static unsigned rx_byte(unsigned nack) {
  unsigned v = 0u, io;
  sda_high();
  for (unsigned i = 0u; i < 8u; i++) {
    dly(HALF);
    scl_high();
    dly(HALF / 2u);
    io = __builtin_ezh_read_gpi();
    v = (v << 1) | sda_lvl(io);
    dly(HALF / 2u);
    scl_low();
  }
  if (nack)
    sda_high();
  else
    sda_low();
  dly(HALF);
  scl_high();
  dly(HALF);
  scl_low();
  sda_high();
  return v;
}

/* 7-bit block write / read. */
static unsigned write_block(unsigned reg, const unsigned char *d, unsigned n) {
  bus_start();
  if (tx_byte((SLAVE_ADDR << 1) | 0u) || tx_byte(reg))
    return 1u;
  for (unsigned i = 0u; i < n; i++)
    if (tx_byte(d[i]))
      return 1u;
  bus_stop();
  return 0u;
}
static unsigned read_block(unsigned reg, unsigned char *out, unsigned n) {
  bus_start();
  if (tx_byte((SLAVE_ADDR << 1) | 0u) || tx_byte(reg))
    return 1u;
  bus_restart();
  if (tx_byte((SLAVE_ADDR << 1) | 1u))
    return 1u;
  for (unsigned i = 0u; i < n; i++)
    out[i] = (unsigned char)rx_byte(i == n - 1u);
  bus_stop();
  return 0u;
}

/* General-call broadcast write [0x00][reg][d..] and reset [0x00][0x06]. */
static unsigned gc_write(unsigned reg, const unsigned char *d, unsigned n) {
  bus_start();
  if (tx_byte(GEN_CALL) || tx_byte(reg))
    return 1u;
  for (unsigned i = 0u; i < n; i++)
    if (tx_byte(d[i]))
      return 1u;
  bus_stop();
  return 0u;
}
static void gc_reset(void) {
  bus_start();
  (void)tx_byte(GEN_CALL);
  (void)tx_byte(0x06u);
  bus_stop();
}

/* 10-bit block write / combined-format read. */
static unsigned write10(unsigned reg, const unsigned char *d, unsigned n) {
  bus_start();
  if (tx_byte(A10_W) || tx_byte(ADDR10_LO) || tx_byte(reg))
    return 1u;
  for (unsigned i = 0u; i < n; i++)
    if (tx_byte(d[i]))
      return 1u;
  bus_stop();
  return 0u;
}
static unsigned read10(unsigned reg, unsigned char *out, unsigned n) {
  bus_start();
  if (tx_byte(A10_W) || tx_byte(ADDR10_LO) || tx_byte(reg))
    return 1u;
  bus_restart();
  if (tx_byte(A10_R))
    return 1u;
  for (unsigned i = 0u; i < n; i++)
    out[i] = (unsigned char)rx_byte(i == n - 1u);
  bus_stop();
  return 0u;
}

static unsigned pack(const unsigned char *b, unsigned n) {
  unsigned v = 0u;
  for (unsigned i = 0u; i < n; i++)
    v |= (unsigned)b[i] << (8u * i);
  return v;
}

int main(void) {
  IOPCTL_PIO2_29 = PIN_SMARTDMA_I2C;
  IOPCTL_PIO2_30 = PIN_SMARTDMA_I2C;
  sda_high();
  scl_high();
  dly(HALF * 8u);

  static const unsigned char blkA[4] = {0x11u, 0x22u, 0x33u, 0x44u};
  static const unsigned char blkB[2] = {0xAAu, 0xBBu};
  static const unsigned char blkC[2] = {0xDEu, 0xADu}; /* general-call payload */
  static const unsigned char blkD[2] = {0xCAu, 0xFEu}; /* 10-bit payload       */
  unsigned fail = 0u;
  unsigned char rb[4] = {0u, 0u, 0u, 0u};

  /* 1. 7-bit write + read-back. */
  if (write_block(8u, blkA, 4u))
    fail |= 0x001u;
  if (write_block(2u, blkB, 2u))
    fail |= 0x002u;
  if (read_block(8u, rb, 4u))
    fail |= 0x004u;
  m_rb_7bit = pack(rb, 4u);
  if (rb[0] != 0x11u || rb[1] != 0x22u || rb[2] != 0x33u || rb[3] != 0x44u)
    fail |= 0x008u;

  /* 2. General-call broadcast write, read back via 7-bit. */
  if (gc_write(20u, blkC, 2u))
    fail |= 0x010u;
  rb[0] = rb[1] = 0u;
  if (read_block(20u, rb, 2u))
    fail |= 0x020u;
  m_rb_gc = pack(rb, 2u);
  if (rb[0] != 0xDEu || rb[1] != 0xADu)
    fail |= 0x040u;

  /* 3. 10-bit write + combined-format read-back. */
  if (write10(12u, blkD, 2u))
    fail |= 0x080u;
  rb[0] = rb[1] = 0u;
  if (read10(12u, rb, 2u))
    fail |= 0x100u;
  m_rb_10bit = pack(rb, 2u);
  if (rb[0] != 0xCAu || rb[1] != 0xFEu)
    fail |= 0x200u;

  /* 4. General-call reset (verified slave-side via i2c_gc_reset). */
  gc_reset();

  m_status = fail;
  return fail ? (int)(0xBAD00000u | fail) : (int)0xCAFEBABEu;
}
