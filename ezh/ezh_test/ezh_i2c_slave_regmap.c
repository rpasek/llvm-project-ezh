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
 * Register-mapped, FREE-RUNNING bit-banged I2C slave on the EZH/SmartDMA core
 * (the successor to ezh_i2c_slave.c). It behaves like a real I2C register
 * device (an EEPROM / a sensor register file) and recognises three address
 * forms in one decode:
 *
 *   - 7-bit address 0x42  : WRITE [addr+W][reg][d0]...  stores into regmap[reg++]
 *                           READ  [addr+R]              streams regmap[ptr++]
 *   - general call (0x00) : [0x00][0x06]            -> general reset (ptr=0)
 *                           [0x00][reg][d0]...      -> BROADCAST write into regmap
 *   - 10-bit address 0x242: WRITE [11110 10 0][0x42][reg][d0]...   (first byte 0xF4)
 *                           READ  [11110 10 0][0x42] Sr [11110 10 1][...]  (0xF5)
 *
 * Common behaviour: loops forever (back-to-back transactions, no re-ignite); the
 * register pointer persists across transactions; STOP and repeated-START are
 * detected (a moving SDA while SCL is high is never a data bit) so multi-byte
 * transfers self-delimit; a 10-bit match made in the write phase carries over a
 * repeated-START into the read phase (the standard combined 10-bit read).
 *
 * Lines are the I3C0 bus on header J18 of the EVK-MIMXRT595 (external pull-ups):
 *   SDA = PIO2_30 = GPI/GPD bit 30,  SCL = PIO2_29 = GPI/GPD bit 29.
 * regmap[] indexing compiles to the register-offset load/store forms
 * (e_ldr_regb / e_str_regb); the hot byte loops stay the minimal read-GPI /
 * test-SCL / branch poll and the register access happens while SCL is low.
 */

#include <stdint.h>

#define SDA_BIT 30u
#define SCL_BIT 29u
#define SDA_MASK (1u << SDA_BIT)
#define SCL_MASK (1u << SCL_BIT)

#define SLAVE_ADDR 0x42u   /* 7-bit address                                    */
#define ADDR10 0x242u      /* 10-bit address                                   */
#define ADDR10_HI ((ADDR10 >> 8) & 0x3u) /* bits[9:8] carried in the 1st byte  */
#define ADDR10_LO (ADDR10 & 0xFFu)        /* bits[7:0] in the 2nd byte          */
#define GEN_CALL 0x00u     /* general-call address                             */
#define GC_RESET 0x06u     /* general-call "reset and reload" command          */
#define NREG 32u
#define REG_MASK (NREG - 1u)

#define IOPCTL_PIO2_29 (*(volatile uint32_t *)0x40004174u)
#define IOPCTL_PIO2_30 (*(volatile uint32_t *)0x40004178u)
#define PIN_SMARTDMA_I2C (0xFu | 0x10u | 0x20u | 0x40u | 0x400u)

/* Debugger-visible state. regmap is the master-accessible register file. */
volatile uint8_t regmap[NREG];
volatile uint32_t i2c_reg_ptr = 0u;
volatile uint32_t i2c_wr_count = 0u;   /* bytes written into regmap (any path) */
volatile uint32_t i2c_rd_count = 0u;   /* bytes streamed back to masters       */
volatile uint32_t i2c_txn_count = 0u;  /* addressed (7-bit/10-bit) transactions */
volatile uint32_t i2c_gc_count = 0u;   /* general-call transactions            */
volatile uint32_t i2c_gc_reset = 0u;   /* general-call resets seen             */
volatile uint32_t i2c_status = 0u;

#define EV_STOP 0x100u
#define EV_RESTART 0x200u

static inline unsigned gpi(void) { return __builtin_ezh_read_gpi(); }

/* Receive one byte MSB-first, watching the first bit for STOP / repeated-START
 * (a moving SDA while SCL is high is not a data bit). Returns 0..255 or an
 * EV_STOP / EV_RESTART event. */
static unsigned recv_byte_or_event(void) {
  unsigned byte = 0u, io, sda;
  for (unsigned b = 0u; b < 8u; b++) {
    while (gpi() & SCL_MASK) /* wait SCL low */
      continue;
    do {
      io = gpi(); /* wait SCL high; this read is also the SDA sample */
    } while (!(io & SCL_MASK));
    sda = (io >> SDA_BIT) & 1u;
    if (b == 0u) {
      for (;;) {
        io = gpi();
        if (!(io & SCL_MASK))
          break; /* SCL fell with SDA steady: 'sda' was a real bit */
        if (((io >> SDA_BIT) & 1u) != sda)
          return sda ? EV_RESTART : EV_STOP;
      }
    }
    byte = (byte << 1) | sda;
  }
  return byte;
}

static void ack(void) {
  while (gpi() & SCL_MASK)
    continue;
  __builtin_ezh_gpd_drive_low(SDA_BIT);
  while (!(gpi() & SCL_MASK))
    continue;
  while (gpi() & SCL_MASK)
    continue;
  __builtin_ezh_gpd_release(SDA_BIT);
}

static void send_byte(unsigned v) {
  for (unsigned b = 0u; b < 8u; b++) {
    while (gpi() & SCL_MASK)
      continue;
    if (v & 0x80u)
      __builtin_ezh_gpd_release(SDA_BIT);
    else
      __builtin_ezh_gpd_drive_low(SDA_BIT);
    v = (v << 1) & 0xFFu;
    while (!(gpi() & SCL_MASK))
      continue;
  }
}

static unsigned read_ack(void) {
  unsigned io;
  while (gpi() & SCL_MASK)
    continue;
  __builtin_ezh_gpd_release(SDA_BIT);
  do {
    io = gpi();
  } while (!(io & SCL_MASK));
  unsigned nack = (io >> SDA_BIT) & 1u;
  while (gpi() & SCL_MASK)
    continue;
  return nack;
}

/* Receive [reg][d0][d1]... into regmap[reg++]. Returns 1 if it ended on a
 * repeated-START (caller re-addresses), 0 on STOP. `count_txn` distinguishes an
 * addressed transfer from a general-call broadcast for the counters. */
static unsigned recv_write_stream(unsigned count_txn) {
  unsigned p = recv_byte_or_event();
  if (p >= EV_STOP) {
    if (count_txn)
      i2c_txn_count++;
    else
      i2c_gc_count++;
    return p == EV_RESTART;
  }
  i2c_reg_ptr = p & REG_MASK;
  ack();
  for (;;) {
    unsigned d = recv_byte_or_event();
    if (d >= EV_STOP) {
      if (count_txn)
        i2c_txn_count++;
      else
        i2c_gc_count++;
      return d == EV_RESTART;
    }
    regmap[i2c_reg_ptr] = (uint8_t)d; /* reg-offset store */
    i2c_reg_ptr = (i2c_reg_ptr + 1u) & REG_MASK;
    i2c_wr_count++;
    ack();
  }
}

/* Stream regmap[ptr++] until the master NACKs. */
static void send_read_stream(void) {
  for (;;) {
    unsigned v = regmap[i2c_reg_ptr]; /* reg-offset load */
    i2c_reg_ptr = (i2c_reg_ptr + 1u) & REG_MASK;
    send_byte(v);
    i2c_rd_count++;
    if (read_ack())
      break;
  }
  i2c_txn_count++;
}

int main(void) {
  IOPCTL_PIO2_29 = PIN_SMARTDMA_I2C;
  IOPCTL_PIO2_30 = PIN_SMARTDMA_I2C;
  __builtin_ezh_gpd_release(SDA_BIT);
  __builtin_ezh_gpd_release(SCL_BIT);

  unsigned ten_sel = 0u; /* matched our full 10-bit address in a write phase */
  for (;;) {
    ten_sel = 0u;
    /* Wait for a START: SDA low while SCL high (idle bus is both-high). */
    unsigned io;
    do {
      io = gpi();
    } while ((io & SDA_MASK) || !(io & SCL_MASK));

  readdr:;
    unsigned a = recv_byte_or_event();
    if (a >= EV_STOP)
      continue;

    if (a == GEN_CALL) {
      /* General call: ACK, read the command/first byte. */
      ack();
      unsigned cmd = recv_byte_or_event();
      if (cmd >= EV_STOP)
        continue;
      ack();
      if (cmd == GC_RESET) {
        i2c_reg_ptr = 0u;
        i2c_gc_reset++;
        i2c_gc_count++;
        i2c_status = 0x47u; /* 'G' */
        continue;           /* master issues STOP */
      }
      /* Otherwise a broadcast register write: cmd is the register pointer. */
      i2c_reg_ptr = cmd & REG_MASK;
      i2c_status = 0x67u; /* 'g' */
      for (;;) {
        unsigned d = recv_byte_or_event();
        if (d >= EV_STOP) {
          i2c_gc_count++;
          if (d == EV_RESTART)
            goto readdr;
          break;
        }
        regmap[i2c_reg_ptr] = (uint8_t)d;
        i2c_reg_ptr = (i2c_reg_ptr + 1u) & REG_MASK;
        i2c_wr_count++;
        ack();
      }
      continue;
    }

    if ((a & 0xF8u) == 0xF0u) {
      /* 10-bit address frame: 11110 XX R/W. */
      unsigned hi = (a >> 1) & 0x3u;
      if ((a & 1u) == 0u) {
        /* Write phase: match high bits, then the low-byte. */
        if (hi != ADDR10_HI)
          continue;
        ack();
        unsigned lo = recv_byte_or_event();
        if (lo >= EV_STOP)
          continue;
        if (lo != ADDR10_LO)
          continue;
        ack();
        ten_sel = 1u;
        i2c_status = 0x41u; /* 'A' */
        if (recv_write_stream(1u))
          goto readdr; /* repeated-START -> the combined read phase */
        continue;
      }
      /* Read phase first byte (11110 XX 1): only valid right after a write-phase
       * match of our full 10-bit address (the combined-format read). */
      if (hi != ADDR10_HI || !ten_sel)
        continue;
      ack();
      send_read_stream();
      continue;
    }

    if ((a >> 1) == SLAVE_ADDR) {
      /* 7-bit address. */
      ack();
      if (a & 1u)
        send_read_stream();
      else if (recv_write_stream(1u))
        goto readdr;
      continue;
    }
    /* Not addressed: leave SDA released (implicit NACK). */
  }
  return 0;
}
