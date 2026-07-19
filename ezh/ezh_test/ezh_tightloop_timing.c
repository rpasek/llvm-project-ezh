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
 * Cycle-accurate timing of tight_loop body shapes, self-measured by the EZH
 * core against CTIMER0's free-running counter (the runner clocks it from
 * FRO_DIV1 and starts it before ignition; EZH reads TC over AHB).
 *
 * The question this answers -- unanswerable from AN14650 -- is whether LOOP
 * ROTATION can pay on this hardware. For a two-instruction pump body
 * [ldr_post; add] the load-use pair sits at issue distance 1 INSIDE the
 * block; rotating to [add; ldr_post] moves the same pair to distance 1
 * ACROSS the hardware loop-back. Rotation therefore helps only if the wrap
 * adds a hidden cycle (turning distance 1 into 2) or the interlock does not
 * track values across block re-entry. Three unknowns, twelve shapes:
 *
 *   w      cycles the hardware loop-back adds on top of the body
 *   s_in   stall of a distance-1 load-use pair inside the block
 *   s_wrap stall of the same pair split across the wrap
 *
 * Each case runs OUTER x tight_loop(INNER) and stores the tick delta. Ticks
 * per core cycle cancel in ratios: q = (alu2 - alu1)/(OUTER*INNER), then
 * cycles/iter = ticks/(q*OUTER*INNER). Outer-loop overhead is < 1% and
 * identical across cases.
 *
 * All bodies are single volatile asm blocks so nothing can be scheduled
 * into them; the run-once slot gets an explicit nop. The runner statically
 * verifies the emitted [tight_loop; nop; body...] sequences instruction by
 * instruction before trusting any number.
 */

#include "ezh_test.h"

#define TC (*(volatile unsigned *)0x40028008) /* CTIMER0->TC */

#define OUTER 50u
#define INNER 2000u

volatile unsigned results[16];
volatile unsigned valid[16]; /* per-case architectural-state check */
static volatile unsigned buf[4096]; /* 16 KB: two disjoint 8 KB halves */

#define INIT0(v) __asm volatile("load_imm %0, 0" : "=r"(v))

/* Pin case-local registers ABOVE the tight_loop intrinsic: without this the
   scheduler is free to place pure pointer/constant materializations after
   TIGHT_LOOP -- i.e. inside the run-once slot or the repeated block (it did:
   the copy case's buf+2048 setup landed in the block). A volatile asm
   reading/writing the registers cannot cross the side-effecting intrinsic. */
#define PIN1(a) __asm volatile("" : "+r"(a))
#define PIN2(a, b) __asm volatile("" : "+r"(a), "+r"(b))
#define PIN3(a, b, c) __asm volatile("" : "+r"(a), "+r"(b), "+r"(c))
#define PIN4(a, b, c, d) __asm volatile("" : "+r"(a), "+r"(b), "+r"(c), "+r"(d))

/* Patterned data + value checks make the dependency evidence ARCHITECTURAL,
   not just timing-based: the rotated cases must deliver the value loaded in
   the PREVIOUS iteration across the wrap, and with distinct per-index data a
   stale or shifted delivery changes sums and destination bytes. All init and
   check loops are plain C, outside the timed windows. */
static unsigned patw(unsigned i) { return (i << 1) + 3; }
static unsigned char patb(unsigned i) { return (unsigned char)(i * 7 + 13); }
static void init_words(void) {
  for (unsigned i = 0; i < INNER; i++)
    buf[i] = patw(i);
}
static void init_bytes(void) {
  volatile unsigned char *b = (volatile unsigned char *)buf;
  for (unsigned i = 0; i < INNER; i++)
    b[i] = patb(i);
}
static unsigned wordsum(unsigned lo, unsigned hi) {
  unsigned s = 0;
  for (unsigned i = lo; i <= hi; i++)
    s += buf[i];
  return s;
}

int main() {
  unsigned t0, t1, k;

  /* Timer sanity: TC must be counting or every delta below is garbage. */
  t0 = TC;
  for (k = 0; k < 64; k++)
    __asm volatile("nop");
  t1 = TC;
  if (t1 == t0)
    return 0xDEAD0000;

  /* [0] alu1: one dependent add_imm. 1 + w cycles/iter. */
  {
    unsigned a;
    INIT0(a);
    t0 = TC;
    for (k = 0; k < OUTER; k++) {
      PIN1(a);
      void *rend = &&x0;
      __builtin_ezh_tight_loop(rend, INNER - 1);
      __asm volatile("nop");
      __asm volatile("add_imm %0, %0, 1" : "+r"(a));
    x0:;
    }
    t1 = TC;
    results[0] = t1 - t0;
    valid[0] = (a == OUTER * INNER) ? 1 : 0xBAD00000;
  }

  /* [1] alu2: two add_imms. 2 + w. The (alu2-alu1) delta is the tick
     quantum for exactly OUTER*INNER cycles. */
  {
    unsigned a;
    INIT0(a);
    t0 = TC;
    for (k = 0; k < OUTER; k++) {
      PIN1(a);
      void *rend = &&x1;
      __builtin_ezh_tight_loop(rend, INNER - 1);
      __asm volatile("nop");
      __asm volatile("add_imm %0, %0, 1\n\tadd_imm %0, %0, 1" : "+r"(a));
    x1:;
    }
    t1 = TC;
    results[1] = t1 - t0;
    valid[1] = (a == 2 * OUTER * INNER) ? 1 : 0xBAD00001;
  }

  /* [2] alu3: three add_imms. 3 + w (linearity check). */
  {
    unsigned a;
    INIT0(a);
    t0 = TC;
    for (k = 0; k < OUTER; k++) {
      PIN1(a);
      void *rend = &&x2;
      __builtin_ezh_tight_loop(rend, INNER - 1);
      __asm volatile("nop");
      __asm volatile("add_imm %0, %0, 1\n\tadd_imm %0, %0, 1\n\tadd_imm %0, %0, 1"
                     : "+r"(a));
    x2:;
    }
    t1 = TC;
    results[2] = t1 - t0;
    valid[2] = (a == 3 * OUTER * INNER) ? 1 : 0xBAD00002;
  }

  /* [3] nop1: is nop 1 cycle like an add? */
  {
    t0 = TC;
    for (k = 0; k < OUTER; k++) {
      void *rend = &&x3;
      __builtin_ezh_tight_loop(rend, INNER - 1);
      __asm volatile("nop");
      __asm volatile("nop");
    x3:;
    }
    t1 = TC;
    results[3] = t1 - t0;
    valid[3] = 1;
  }

  /* [4] load_use, the UNROTATED sum pump: [ldr_post; add] with the
     consumer at distance 1 inside the block. 2 + w + s_in. */
  {
    unsigned a, d;
    volatile unsigned *p;
    INIT0(a);
    INIT0(d);
    init_words();
    t0 = TC;
    for (k = 0; k < OUTER; k++) {
      p = buf;
      PIN3(p, d, a);
      void *rend = &&x4;
      __builtin_ezh_tight_loop(rend, INNER - 1);
      __asm volatile("nop");
      __asm volatile("ldr_post %0, %1, 4\n\tadd %2, %1, %2"
                     : "+r"(p), "+r"(d), "+r"(a)::"memory");
    x4:;
    }
    t1 = TC;
    results[4] = t1 - t0;
    valid[4] = (p == buf + INNER && a == OUTER * wordsum(0, INNER - 1) &&
                d == patw(INNER - 1))
                   ? 1
                   : 0xBAD00004;
  }

  /* [5] load_rot, the ROTATED sum pump: [add; ldr_post] with the same
     pair at distance 1 across the wrap. 2 + w + s_wrap. */
  {
    unsigned a, d;
    volatile unsigned *p;
    INIT0(a);
    INIT0(d);
    init_words();
    t0 = TC;
    for (k = 0; k < OUTER; k++) {
      p = buf;
      PIN3(p, d, a);
      void *rend = &&x5;
      __builtin_ezh_tight_loop(rend, INNER - 1);
      __asm volatile("nop");
      __asm volatile("add %2, %1, %2\n\tldr_post %0, %1, 4"
                     : "+r"(p), "+r"(d), "+r"(a)::"memory");
    x5:;
    }
    t1 = TC;
    results[5] = t1 - t0;
    /* Each add consumes the value loaded ONE ITERATION EARLIER, across the
       hardware wrap: per pass the sum is d_entry + buf[0..INNER-2], where
       d_entry is 0 on the first pass (INIT0) and buf[INNER-1] carried over
       on every later pass. A stale or dropped cross-wrap delivery breaks
       this exact total. */
    valid[5] = (p == buf + INNER &&
                a == OUTER * wordsum(0, INNER - 2) +
                         (OUTER - 1) * patw(INNER - 1) &&
                d == patw(INNER - 1))
                   ? 1
                   : 0xBAD00005;
  }

  /* [6] load_space: consumer at distance 2 inside the block.
     3 + w + s_dist2 (the interlock model says s_dist2 = 0). */
  {
    unsigned a, d, x;
    volatile unsigned *p;
    INIT0(a);
    INIT0(d);
    INIT0(x);
    init_words();
    t0 = TC;
    for (k = 0; k < OUTER; k++) {
      p = buf;
      PIN4(p, d, a, x);
      void *rend = &&x6;
      __builtin_ezh_tight_loop(rend, INNER - 1);
      __asm volatile("nop");
      __asm volatile("ldr_post %0, %1, 4\n\tadd_imm %3, %3, 1\n\tadd %2, %1, %2"
                     : "+r"(p), "+r"(d), "+r"(a), "+r"(x)::"memory");
    x6:;
    }
    t1 = TC;
    results[6] = t1 - t0;
    valid[6] = (p == buf + INNER && x == OUTER * INNER &&
                a == OUTER * wordsum(0, INNER - 1))
                   ? 1
                   : 0xBAD00006;
  }

  /* [7] load_only: back-to-back unconsumed loads -- raw AHB issue rate. */
  {
    unsigned d;
    volatile unsigned *p;
    INIT0(d);
    t0 = TC;
    for (k = 0; k < OUTER; k++) {
      p = buf;
      PIN2(p, d);
      void *rend = &&x7;
      __builtin_ezh_tight_loop(rend, INNER - 1);
      __asm volatile("nop");
      __asm volatile("ldr_post %0, %1, 4" : "+r"(p), "+r"(d)::"memory");
    x7:;
    }
    t1 = TC;
    results[7] = t1 - t0;
    valid[7] = (p == buf + INNER && d == patw(INNER - 1)) ? 1 : 0xBAD00007;
  }

  /* [8] store_only: back-to-back stores -- raw store issue rate. */
  {
    unsigned d;
    volatile unsigned *p;
    d = 0x5A17u;
    t0 = TC;
    for (k = 0; k < OUTER; k++) {
      p = buf;
      PIN2(p, d);
      void *rend = &&x8;
      __builtin_ezh_tight_loop(rend, INNER - 1);
      __asm volatile("nop");
      __asm volatile("str_post %0, %1, 4" : "+r"(p) : "r"(d) : "memory");
    x8:;
    }
    t1 = TC;
    results[8] = t1 - t0;
    valid[8] = (p == buf + INNER) ? 1 : 0xBAD00008;
    for (unsigned i = 0; i < INNER; i++)
      if (buf[i] != 0x5A17u)
        valid[8] = 0xBAD10008;
  }

  /* [9] copy, the UNROTATED memcpy pump: [ldrb_post; strb_post], the
     store consumes the load at distance 1 inside the block. */
  {
    unsigned d;
    volatile unsigned char *ps, *pd;
    INIT0(d);
    init_bytes();
    t0 = TC;
    for (k = 0; k < OUTER; k++) {
      ps = (volatile unsigned char *)buf;
      pd = (volatile unsigned char *)(buf + 2048);
      PIN3(ps, pd, d);
      void *rend = &&x9;
      __builtin_ezh_tight_loop(rend, INNER - 1);
      __asm volatile("nop");
      /* byte loads are DATA-first (unlike ldr_post/str_post): the loaded
         byte lands in %2 and the pointer %0 takes the writeback */
      __asm volatile("ldrb_post %2, %0, 1\n\tstrb_post %1, %2, 1"
                     : "+r"(ps), "+r"(pd), "+r"(d)::"memory");
    x9:;
    }
    t1 = TC;
    results[9] = t1 - t0;
    valid[9] = (ps == (unsigned char *)buf + INNER &&
                pd == (unsigned char *)(buf + 2048) + INNER)
                   ? 1
                   : 0xBAD00009;
    for (unsigned i = 0; i < INNER; i++)
      if (((volatile unsigned char *)(buf + 2048))[i] != patb(i))
        valid[9] = 0xBAD10009;
  }

  /* [10] copy_rot: [strb_post; ldrb_post], the same pair across the wrap. */
  {
    unsigned d;
    volatile unsigned char *ps, *pd;
    INIT0(d);
    t0 = TC;
    for (k = 0; k < OUTER; k++) {
      ps = (volatile unsigned char *)buf;
      pd = (volatile unsigned char *)(buf + 2048);
      PIN3(ps, pd, d);
      void *rend = &&x10;
      __builtin_ezh_tight_loop(rend, INNER - 1);
      __asm volatile("nop");
      __asm volatile("strb_post %1, %2, 1\n\tldrb_post %2, %0, 1"
                     : "+r"(ps), "+r"(pd), "+r"(d)::"memory");
    x10:;
    }
    t1 = TC;
    results[10] = t1 - t0;
    valid[10] = (ps == (unsigned char *)buf + INNER &&
                 pd == (unsigned char *)(buf + 2048) + INNER &&
                 d == patb(INNER - 1))
                    ? 1
                    : 0xBAD0000A;
    /* The rotated body stores the byte loaded ONE ITERATION EARLIER across
       the wrap: the final pass leaves dst shifted by one, with dst[0] being
       the LAST byte loaded on the previous pass. A stale or dropped
       cross-wrap delivery breaks the shift pattern. */
    {
      volatile unsigned char *dst = (volatile unsigned char *)(buf + 2048);
      if (dst[0] != patb(INNER - 1))
        valid[10] = 0xBAD1000A;
      for (unsigned i = 1; i < INNER; i++)
        if (dst[i] != patb(i - 1))
          valid[10] = 0xBAD2000A;
    }
  }

  /* [11] load2: two independent back-to-back loads -- does the AHB/DFETCH
     path pipeline distinct transactions? */
  {
    unsigned d, d2;
    volatile unsigned *p, *p2;
    INIT0(d);
    INIT0(d2);
    init_words();
    for (unsigned i = 0; i < INNER; i++)
      buf[2048 + i] = patw(i) ^ 0x00A50000u;
    t0 = TC;
    for (k = 0; k < OUTER; k++) {
      p = buf;
      p2 = buf + 2048;
      PIN4(p, p2, d, d2);
      void *rend = &&x11;
      __builtin_ezh_tight_loop(rend, INNER - 1);
      __asm volatile("nop");
      __asm volatile("ldr_post %0, %2, 4\n\tldr_post %1, %3, 4"
                     : "+r"(p), "+r"(p2), "+r"(d), "+r"(d2)::"memory");
    x11:;
    }
    t1 = TC;
    results[11] = t1 - t0;
    valid[11] = (p == buf + INNER && p2 == buf + 2048 + INNER &&
                 d == patw(INNER - 1) &&
                 d2 == (patw(INNER - 1) ^ 0x00A50000u))
                    ? 1
                    : 0xBAD0000B;
  }

  /* [12..14] ORDINARY software loops (backedge inside one asm block, so
     the compiler cannot touch them): what does tight_loop actually buy over
     add_imms + goto_nz for the three canonical pumps? */
  {
    unsigned d, c;
    volatile unsigned *p;
    d = 0x3C99u;
    t0 = TC;
    for (k = 0; k < OUTER; k++) {
      p = buf;
      c = INNER;
      __asm volatile("1:\n\tstr_post %0, %1, 4\n\tadd_imms %2, %2, -1\n\tgoto_nz 1b"
                     : "+r"(p), "+r"(d), "+r"(c)::"memory");
    }
    t1 = TC;
    results[12] = t1 - t0;
    valid[12] = (p == buf + INNER && c == 0) ? 1 : 0xBAD0000C;
    for (unsigned i = 0; i < INNER; i++)
      if (buf[i] != 0x3C99u)
        valid[12] = 0xBAD1000C;
  }
  {
    unsigned a, d, c;
    volatile unsigned *p;
    INIT0(a);
    INIT0(d);
    init_words();
    t0 = TC;
    for (k = 0; k < OUTER; k++) {
      p = buf;
      c = INNER;
      __asm volatile("1:\n\tldr_post %0, %1, 4\n\tadd %2, %1, %2\n\tadd_imms %3, %3, -1\n\tgoto_nz 1b"
                     : "+r"(p), "+r"(d), "+r"(a), "+r"(c)::"memory");
    }
    t1 = TC;
    results[13] = t1 - t0;
    valid[13] = (p == buf + INNER && c == 0 &&
                 a == OUTER * wordsum(0, INNER - 1) && d == patw(INNER - 1))
                    ? 1
                    : 0xBAD0000D;
  }
  {
    unsigned d, c;
    volatile unsigned char *ps, *pd;
    INIT0(d);
    init_bytes();
    t0 = TC;
    for (k = 0; k < OUTER; k++) {
      ps = (volatile unsigned char *)buf;
      pd = (volatile unsigned char *)(buf + 2048);
      c = INNER;
      __asm volatile("1:\n\tldrb_post %2, %0, 1\n\tstrb_post %1, %2, 1\n\tadd_imms %3, %3, -1\n\tgoto_nz 1b"
                     : "+r"(ps), "+r"(pd), "+r"(d), "+r"(c)::"memory");
    }
    t1 = TC;
    results[14] = t1 - t0;
    valid[14] = (ps == (unsigned char *)buf + INNER && c == 0) ? 1 : 0xBAD0000E;
    for (unsigned i = 0; i < INNER; i++)
      if (((volatile unsigned char *)(buf + 2048))[i] != patb(i))
        valid[14] = 0xBAD1000E;
  }

  /* [15] perload_only: back-to-back loads from a PERIPHERAL (CTIMER0 TC on
     the APB bridge) instead of the local SRAM the code executes from --
     separates instruction-fetch contention from bus latency. */
  {
    unsigned d;
    volatile unsigned *p = (volatile unsigned *)0x40028008;
    INIT0(d);
    t0 = TC;
    for (k = 0; k < OUTER; k++) {
      PIN2(p, d);
      void *rend = &&x15;
      __builtin_ezh_tight_loop(rend, INNER - 1);
      __asm volatile("nop");
      __asm volatile("ldr %1, %0, 0" : "+r"(p), "+r"(d)::"memory");
    x15:;
    }
    t1 = TC;
    results[15] = t1 - t0;
    valid[15] = (d != 0) ? 1 : 0xBAD0000F;
  }

  return 0xCAFEBABE;
}
