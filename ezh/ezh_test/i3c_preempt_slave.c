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
 * PREEMPTIVE-MODEL I3C SLAVE: tiling as mainline code, I3C as a real ISR.
 *
 * This is the OTHER interrupt model (vs. the hold-based demos): compiled
 * WITH bitslice interrupts (no -mno-ezh-bitslice-interrupts), the backend
 * injects a conditional `gotol_bs bitslice_handler` before every direct
 * branch/call, so this mainline code becomes interrupt-eligible at every
 * branch boundary. crt0's bitslice_handler saves context, masks the slice
 * enables, dispatches every pending slice to its weak vectorN() C function,
 * and restores CFM (which re-arms) on exit.
 *
 *   slice 0 <- trig ch 0 <- I3C0 IRQ (via INPUTMUX)  ->  vector0() ISR
 *
 * main() never mentions I3C at all: it just copies tiles round-robin, flat
 * out -- the stand-in for the SmartDMA's real tiling job. Meanwhile the
 * master board runs the full true-I3C ceremony (RSTDAA, ENTDAA assigning
 * dynamic address 0x30, 17-byte private SDR write) and every bus event
 * PREEMPTS the tile loop into vector0(). s_tiles proves the mainline kept
 * working throughout; s_isr counts the preemptions.
 *
 * ISR discipline for a level IRQ under this model: vector0() must service
 * the peripheral until the line drops (drain RX, W1C status) before
 * returning -- the handler's CFM restore clears the sticky flags, and a
 * still-high line simply re-latches and re-enters at the next branch.
 * Do not touch CFM inside a vectorN(): the handler owns it there.
 *
 * Exits 0 -> exc_signal 0xCAFEBABE once the I3C payload arrived and a
 * healthy number of tiles have been copied.
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

#define NDATA 17u    /* payload of i3c_sdr_master.c              */
#define NTILES 3u    /* distinct tiles in the rotating workload  */
#define TILE 16u
#define MIN_TILES 50u /* mainline work to prove before exiting   */

/* Bitslice combiner encoding (see EVENT_FABRIC.md). */
#define BS0(c) ((c) << 8)
#define BS1(c) ((c) << 11)
#define BS2(c) ((c) << 14)
#define BS3(c) ((c) << 17)
#define BS4(c) ((c) << 20)
#define BS5(c) ((c) << 23)
#define BS6(c) ((c) << 26)
#define BS7(c) ((c) << 29)
#define BS_SIG 4u /* level-high detect (the I3C IRQ is a level) */
#define BS_0 6u   /* force 0 (unused slices keep the combiner low) */
#define REST_FORCE0                                                            \
  (BS1(BS_0) | BS2(BS_0) | BS3(BS_0) | BS4(BS_0) | BS5(BS_0) | BS6(BS_0) |     \
   BS7(BS_0))

volatile unsigned s_isr = 0, s_rx = 0, s_stops = 0, s_dachg = 0, s_dyn = 0;
volatile unsigned s_tiles = 0, s_tiles_at_done = 0, s_mark = 0;
volatile unsigned s_log[24]; /* ISR entry ring: (SSTATUS, s_tiles) pairs */
volatile unsigned char s_buf[24];
volatile unsigned char tile_src[NTILES * TILE];
volatile unsigned char tile_dst[NTILES * TILE];

/* The I3C interrupt service routine: overrides crt0's weak stub and is
 * invoked BY PREEMPTION whenever the I3C0 IRQ asserts while the tile loop
 * runs. Plain C; crt0's handler has already saved context. */
void __attribute__((used)) vector0(void) {
  if (s_isr < 12u) {
    s_log[s_isr * 2u] = R(SSTATUS);
    s_log[s_isr * 2u + 1u] = s_tiles;
  }
  s_isr++;
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
    if (s_rx >= NDATA)
      s_tiles_at_done = s_tiles; /* snapshot: mainline progress so far */
  }
}

int main(void) {
  for (unsigned i = 0u; i < sizeof tile_src; i++)
    tile_src[i] = (unsigned char)(0xA0u + i);

  /* Arm slice 0 on the routed I3C0 IRQ (level detect), everything else
   * forced 0, enables on -- from here on, any branch in the code below is
   * an interrupt entry point. */
  __builtin_ezh_write_cfs(BS0(0u));
  __builtin_ezh_write_cfm(BS0(BS_SIG) | REST_FORCE0 | 0xFFu);
  __builtin_ezh_write_cfm(__builtin_ezh_read_cfm()); /* clear stale flags */

  /* The "tiling" mainline: copy tiles round-robin, flat out. No I3C code
   * anywhere in this loop -- the ISR preempts it. */
  for (;;) {
    unsigned t = s_tiles % NTILES;
    unsigned base = t * TILE;
    for (unsigned i = 0u; i < TILE; i++)
      tile_dst[base + i] = tile_src[base + i];
    s_tiles = s_tiles + 1u;

    if (s_rx >= NDATA && s_stops != 0u && s_tiles >= MIN_TILES)
      break;
  }
  s_mark = 0x77u; /* reached ONLY if the loop exits cleanly */
  return 0;       /* crt0 -> exc_signal = 0xCAFEBABE */
}
