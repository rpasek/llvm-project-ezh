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
 * Bit-banged I2C MASTER on the EZH/SmartDMA core, compiled by ezh-none-elf.
 * Counterpart to ezh_i2c_slave.c; runs on the second EVK-MIMXRT595, wired
 * J18<->J18:
 *   SDA = PIO2_30 = bit 30, SCL = PIO2_29 = bit 29  (I3C0 bus on J18)
 * Drives a single write transaction to address 0x42: START, address+W, one
 * data byte (0xA5), STOP, sampling the ACK after each byte. The master owns
 * the clock so timing is just fixed delays (slow enough that the polled slave
 * never misses an edge). Returns 0xCAFEBABE iff both bytes were ACKed.
 */

#include <stdint.h>

#define SDA_BIT 30u /* PIO2_30 / I3C0_SDA / J18 */
#define SCL_BIT 29u /* PIO2_29 / I3C0_SCL / J18 */

#define SLAVE_ADDR 0x42u

/* Overridable from the build for the speed sweep / payload tracking:
 *   -DHALF=<n>     half-bit delay in loop iters (smaller => faster bus)
 *   -DWR_BYTE=<n>  data byte the master writes
 *   -DREPEAT=<n>   back-to-back transactions (n>1 = timing mode, ignores ACK) */
#ifndef WR_BYTE
#define WR_BYTE 0xA5u
#endif
#ifndef HALF
/* HALF=100 => ~104 kHz SCL (standard-mode I2C) on this EVK; measured ceiling is
 * HALF=32 (~415 kHz, fast-mode) before the polled slave starts missing edges. */
#define HALF 100u
#endif
#ifndef REPEAT
#define REPEAT 1
#endif

/* IOPCTL: PIO2_29 -> 0x40004174, PIO2_30 -> 0x40004178; SmartDMA = Func 15. */
#define IOPCTL_PIO2_29 (*(volatile uint32_t *)0x40004174u)
#define IOPCTL_PIO2_30 (*(volatile uint32_t *)0x40004178u)
#define PIN_SMARTDMA_I2C                                                        \
  (0xFu /*FSEL=15*/ | 0x10u /*PUPDENA*/ | 0x20u /*PUPDSEL=up*/ |                \
   0x40u /*IBENA*/ | 0x400u /*ODENA*/)

volatile uint32_t m_status = 0u;

static inline unsigned sda_lvl(unsigned io) { return (io >> SDA_BIT) & 1u; }

static void dly(unsigned n) {
  while (n--)
    __asm__ volatile("");
}

/* Open-drain: release() floats the line high (pull-up); drive_low() pulls it. */
static void scl_high(void) { __builtin_ezh_gpd_release(SCL_BIT); }
static void scl_low(void) { __builtin_ezh_gpd_drive_low(SCL_BIT); }
static void sda_high(void) { __builtin_ezh_gpd_release(SDA_BIT); }
static void sda_low(void) { __builtin_ezh_gpd_drive_low(SDA_BIT); }

/* Clock out one byte, MSB first; return 0 if the slave ACKed, 1 if NACK. */
static unsigned tx_byte(unsigned b) {
  for (unsigned i = 0u; i < 8u; i++) {
    if (b & 0x80u)
      sda_high();
    else
      sda_low();
    b = (b << 1) & 0xFFu;
    dly(HALF);
    scl_high(); /* slave samples SDA on this rising edge */
    dly(HALF);
    scl_low();
  }
  /* 9th clock: release SDA and sample the slave's ACK. */
  sda_high();
  dly(HALF);
  scl_high();
  dly(HALF / 2u);
  unsigned ack = sda_lvl(__builtin_ezh_read_gpi()); /* 0 = ACK */
  dly(HALF / 2u);
  scl_low();
  return ack;
}

int main(void) {
  IOPCTL_PIO2_29 = PIN_SMARTDMA_I2C;
  IOPCTL_PIO2_30 = PIN_SMARTDMA_I2C;

  /* Idle high, then let the bus settle / the slave come up. */
  sda_high();
  scl_high();
  dly(HALF * 8u);

  unsigned nack_addr = 1u, nack_data = 1u;
  for (unsigned r = 0u; r < (unsigned)REPEAT; r++) {
    /* START: SDA high->low while SCL high. */
    sda_low();
    dly(HALF);
    scl_low();
    dly(HALF);

    nack_addr = tx_byte((SLAVE_ADDR << 1) | 0u); /* address + W */
    nack_data = tx_byte(WR_BYTE);

    /* STOP: SDA low while SCL low, raise SCL, then release SDA. */
    sda_low();
    dly(HALF);
    scl_high();
    dly(HALF);
    sda_high();
    dly(HALF);
  }

  m_status = (nack_addr << 1) | nack_data;
#if REPEAT > 1
  return (int)0xCAFEBABEu; /* timing mode: drove REPEAT transactions */
#else
  if (nack_addr)
    return (int)0xBAD0A000u; /* nobody ACKed the address */
  if (nack_data)
    return (int)0xBAD0D000u; /* address ACKed but data NACKed */
  return (int)0xCAFEBABEu;   /* slave ACKed address + data */
#endif
}
