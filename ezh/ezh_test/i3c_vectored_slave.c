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
 * TWO-SOURCE VECTORED DISPATCH ("NVIC emulation") on the EZH -- one core
 * serving two independent interrupt sources concurrently, no polling:
 *
 *   slice 0 <- trig ch 0 <- I3C0 IRQ (via INPUTMUX)   -> i3c "ISR"
 *   slice 1 <- trig ch 1 <- software doorbell          -> tile "ISR"
 *              (PENDTRAP, standing in for a tiling/display trigger --
 *               any INPUTMUX-routable source works the same way)
 *
 * The core sleeps in __builtin_ezh_acc_vectored_hold(base, mask). On a wake
 * won by an armed slice n whose mask bit is set, the HARDWARE computes the
 * vector base + 4 + 4*n and hands it back -- that is the NVIC's "vector
 * fetch". We derive n from it and run the matching C handler, which is the
 * integration-friendly form (handlers are plain C functions). The raw
 * hardware-GOTO form -- pass a real instruction table and land on
 * table + 4 + 4*n with rDest = PC, no jump instruction in the firmware --
 * is proven byte-exact in ezh_vhold_dispatch.c; both forms are the same
 * instruction, differing only in which register receives the vector.
 *
 * The I3C side is the M3 slave (i3c_sdr_slave.c): true I3C SDR, the master
 * runs RSTDAA + ENTDAA (we see DACHG twice) and then a 17-byte private SDR
 * write. The tile side simulates the real use case (SmartDMA doing display
 * tiling while also serving I3C): each doorbell copies the next 16-byte tile
 * from tile_src[] to tile_dst[], which the runner verifies byte-for-byte.
 *
 * One fabric caveat worth knowing for production designs: the full-CFM write
 * that re-arms after a wake clears the sticky flags of ALL slices, so an
 * event of source B latched while source A is being serviced can be lost if
 * B's line is no longer asserted. The I3C side is immune (its IRQ is a level
 * and simply re-wakes); for pulse-like sources, either keep the service path
 * short, re-check the source after re-arming, or use a level-style handshake.
 *
 * Exits 0 -> exc_signal 0xCAFEBABE once 17 I3C bytes + a STOP + 3 tiles are
 * in. s_vec0/s_vec1 count hardware-vectored wakes per source.
 */

#include "ezh_test.h"

#define I3C0 0x40036000u
#define R(o) (*(volatile unsigned *)(I3C0 + (o)))
#define SSTATUS 0x08u
#define SRDATAB 0x40u
#define SDYNADDR 0x64u

#define ST_STOP 0x400u
#define ST_RXPEND 0x800u
#define ST_DACHG 0x2000u

#define NDATA 17u  /* payload of i3c_sdr_master.c */
#define NTILES 3u  /* doorbells the runner fires   */
#define TILE 16u

/* Pure vector base: nothing jumps through it (the builtin's rDest is a GPR,
 * not PC), so any value works; the hardware returns VH_BASE + 4 + 4*slice. */
#define VH_BASE 0x1000u

/* Bitslice combiner encoding (see EVENT_FABRIC.md). */
#define BS0(c) ((c) << 8)
#define BS1(c) ((c) << 11)
#define BS2(c) ((c) << 14)
#define BS3(c) ((c) << 17)
#define BS4(c) ((c) << 20)
#define BS5(c) ((c) << 23)
#define BS6(c) ((c) << 26)
#define BS7(c) ((c) << 29)
#define BS_RISE 1u /* sticky rising-edge detect (the doorbell is a pulse)  */
#define BS_SIG 4u  /* level-high detect (the I3C IRQ is a level)           */
#define BS_0 6u    /* force 0 (unused slices keep the combiner low)        */
#define REST_FORCE0                                                            \
  (BS2(BS_0) | BS3(BS_0) | BS4(BS_0) | BS5(BS_0) | BS6(BS_0) | BS7(BS_0))

volatile unsigned s_vec0 = 0, s_vec1 = 0, s_spur = 0;
volatile unsigned s_rx = 0, s_stops = 0, s_dachg = 0, s_dyn = 0, s_tiles = 0;
volatile unsigned char s_buf[24];
volatile unsigned char tile_src[NTILES * TILE];
volatile unsigned char tile_dst[NTILES * TILE];

int main(void) {
  /* The "tiling" source data: a recognizable pattern the runner checks. */
  for (unsigned i = 0u; i < sizeof tile_src; i++)
    tile_src[i] = (unsigned char)(0xA0u + i);

  /* Arm the two slices: slice 0 = I3C IRQ (level), slice 1 = doorbell
   * (sticky rising edge); everything else forced 0. */
  unsigned cfm = BS0(BS_SIG) | BS1(BS_RISE) | REST_FORCE0 | 0xFFu;
  __builtin_ezh_write_cfs(BS0(0u) | BS1(1u));
  __builtin_ezh_write_cfm(cfm);
  __builtin_ezh_write_cfm(__builtin_ezh_read_cfm()); /* clear stale flags */

  for (;;) {
    /* Sleep until either source fires; the hardware computes which. */
    void *vec = __builtin_ezh_acc_vectored_hold((void *)VH_BASE, 0x3u);
    unsigned slice = (((unsigned)vec - VH_BASE) >> 2) - 1u;

    if (slice == 0u) { /* ---- the I3C "ISR" ---- */
      s_vec0++;
      unsigned st = R(SSTATUS);
      while (st & ST_RXPEND) {
        unsigned b = R(SRDATAB);
        if (s_rx < sizeof s_buf)
          s_buf[s_rx] = (unsigned char)b;
        s_rx = s_rx + 1u;
        st = R(SSTATUS);
      }
      if (st & ST_DACHG) {
        s_dyn = R(SDYNADDR);
        s_dachg = s_dachg + 1u;
        R(SSTATUS) = ST_DACHG;
      }
      if (st & ST_STOP) {
        R(SSTATUS) = ST_STOP;
        s_stops = s_stops + 1u;
      }
    } else if (slice == 1u) { /* ---- the tile "ISR" ---- */
      s_vec1++;
      if (s_tiles < NTILES) {
        unsigned base = s_tiles * TILE;
        for (unsigned i = 0u; i < TILE; i++)
          tile_dst[base + i] = tile_src[base + i];
      }
      s_tiles = s_tiles + 1u;
    } else {
      s_spur++; /* masked-out / unexpected wake: tolerated, counted */
    }

    __builtin_ezh_write_cfm(cfm); /* re-arm (full write clears sticky) */

    if (s_rx >= NDATA && s_stops != 0u && s_tiles >= NTILES)
      break;
  }
  return 0; /* crt0 -> exc_signal = 0xCAFEBABE */
}
