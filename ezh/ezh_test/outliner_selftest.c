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
 * On-board differential self-test for the MachineOutliner. run_outliner.sh
 * builds this twice -- outliner ON and OFF -- and checks that the result
 * table is byte-identical and matches a host-computed golden. Any
 * divergence means an outlined sequence changed behavior (a whitelist
 * miscompile). The MIX kernel is a long run of pure register ALU shared
 * across many framed functions, so the outliner fires; the probes plant
 * live values around the shared kernel where a bad clobber would show.
 */

volatile unsigned g_results[40];
volatile unsigned g_sink;

/* Force the shared kernel to be framed (RA saved) so it is outlinable:
 * each wrapper makes a real call. The kernel is pure register ALU. */
static void __attribute__((noinline)) use(unsigned v) { g_sink = v; }

#define MIX(a, b, c, d)                                                        \
  do {                                                                        \
    x = (a ^ b);                                                              \
    x = (x & c) | d;                                                          \
    x = (x << 3) ^ (x >> 5);                                                  \
    x = (x + a) - b;                                                          \
    x = (x | c) & d;                                                          \
    x = (x << 7) | (x >> 9);                                                  \
    x = (x ^ a) + (c << 2);                                                   \
    x = (x - d) ^ (b << 1);                                                   \
  } while (0)

/* 24 framed functions sharing the identical MIX kernel. */
#define K(N)                                                                   \
  static unsigned __attribute__((noinline)) k##N(unsigned a, unsigned b,       \
                                                 unsigned c, unsigned d) {     \
    unsigned x;                                                                \
    MIX(a, b, c, d);                                                           \
    use(x);                                                                    \
    return x;                                                                  \
  }
K(0) K(1) K(2) K(3) K(4) K(5) K(6) K(7) K(8) K(9) K(10) K(11)
K(12) K(13) K(14) K(15) K(16) K(17) K(18) K(19) K(20) K(21) K(22) K(23)

typedef unsigned (*fn)(unsigned, unsigned, unsigned, unsigned);
static fn table[24] = {k0,  k1,  k2,  k3,  k4,  k5,  k6,  k7,
                       k8,  k9,  k10, k11, k12, k13, k14, k15,
                       k16, k17, k18, k19, k20, k21, k22, k23};

/* Probe: R0-R3 live-through. Run the shared kernel on COPIES, then combine
 * with the untouched originals -- a bad over-clobber of R0-R3 by the
 * outline call would corrupt this. */
static unsigned __attribute__((noinline)) probe_livethrough(unsigned a,
                                                            unsigned b,
                                                            unsigned c,
                                                            unsigned d) {
  unsigned x;
  MIX(a, b, c, d);
  use(x);
  return x + a - b + c - d; /* originals must survive the outlined call */
}

int main(void) {
  volatile unsigned s = 0x1234u;
  for (unsigned i = 0; i < 24u; i++) {
    unsigned a = s + i, b = s ^ (i * 7u), c = (i << 3) + 5u, d = ~i;
    g_results[i] = table[i](a, b, c, d);
  }
  g_results[24] = probe_livethrough(0xAAAAu, 0x5555u, 0x0F0Fu, 0xF0F0u);
  g_results[25] = probe_livethrough(1u, 2u, 3u, 4u);
  return 0; /* crt0 -> exc_signal = 0xCAFEBABE */
}
