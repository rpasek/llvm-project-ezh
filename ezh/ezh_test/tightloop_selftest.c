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
 * Runtime self-test for COMPILER-FORMED tight_loop hardware loops
 * (EZHTightLoopFormation). Built at -O2 with -mno-ezh-bitslice-interrupts so
 * the pass fires; the runner script verifies the tight_loops are actually
 * present in the binary, then executes on silicon and checks results.
 *
 * Counts come from a volatile source so every loop is a genuine
 * runtime-count loop (nothing constant-folds or unrolls).
 */

volatile unsigned n_src;

static volatile unsigned fill_buf[70];
static volatile unsigned char copy_dst[70];
static unsigned char copy_src[70];
static unsigned sum_src[70];
static volatile unsigned cd_buf[70];

/* results readable over JTAG for post-mortem */
volatile unsigned g_sum1, g_sum7, g_sum64;

__attribute__((noinline)) static void fill(volatile unsigned *p, unsigned x,
                                           int n) {
  for (int i = 0; i < n; i++)
    *p++ = x;
}

__attribute__((noinline)) static void copy(volatile unsigned char *d,
                                           const unsigned char *s, int n) {
  for (int i = 0; i < n; i++)
    *d++ = *s++;
}

__attribute__((noinline)) static unsigned sum(const unsigned *a, int n) {
  unsigned s = 0;
  for (int i = 0; i < n; i++)
    s += a[i];
  return s;
}

/* The shared-counter shape the pass must NOT convert -- correctness of the
   ordinary path in the same binary. */
__attribute__((noinline)) static void countdown(volatile unsigned *p,
                                                unsigned n) {
  for (unsigned i = n; i > 0; i--)
    *p++ = i;
}

static int check_fill(unsigned n, unsigned x) {
  for (unsigned i = 0; i < n; i++)
    if (fill_buf[i] != x)
      return 0;
  return fill_buf[n] == 0x5A5A5A5A; /* sentinel untouched */
}

int main() {
  {
    unsigned c3 = 1, sq = 0; /* i*3+1 and i*i built additively (no __mulsi3) */
    for (unsigned i = 0; i < 70; i++) {
      fill_buf[i] = 0x5A5A5A5A;
      copy_dst[i] = 0xA5;
      copy_src[i] = (unsigned char)c3;
      sum_src[i] = sq + 7;
      cd_buf[i] = 0x5A5A5A5A;
      c3 += 3;
      sq += 2 * i + 1; /* (i+1)^2 = i^2 + 2i + 1; 2*i is a shift */
    }
  }

  /* fill: n = 1, 7, 64 */
  n_src = 1;
  fill(fill_buf, 0x11111111, (int)n_src);
  if (!check_fill(1, 0x11111111))
    return 0xE0000001;
  n_src = 7;
  fill(fill_buf, 0x22222222, (int)n_src);
  if (!check_fill(7, 0x22222222))
    return 0xE0000007;
  n_src = 64;
  fill(fill_buf, 0x33333333, (int)n_src);
  if (!check_fill(64, 0x33333333))
    return 0xE0000064;

  /* copy: n = 7 and 64, byte-exact with sentinel */
  n_src = 7;
  copy(copy_dst, copy_src, (int)n_src);
  for (unsigned i = 0; i < 7; i++)
    if (copy_dst[i] != copy_src[i])
      return 0xE1000000 | i;
  if (copy_dst[7] != 0xA5)
    return 0xE10000A5;
  n_src = 64;
  copy(copy_dst, copy_src, (int)n_src);
  for (unsigned i = 0; i < 64; i++)
    if (copy_dst[i] != copy_src[i])
      return 0xE1100000 | i;

  /* sum: load-carrying body */
  n_src = 1;
  g_sum1 = sum(sum_src, (int)n_src);
  if (g_sum1 != 7)
    return 0xE2000001;
  n_src = 7;
  g_sum7 = sum(sum_src, (int)n_src);
  /* sum i*i+7 for i=0..6 = 91 + 49 = 0+1+4+9+16+25+36=91, +7*7=49 -> 140 */
  if (g_sum7 != 140)
    return 0xE2000007;
  n_src = 64;
  g_sum64 = sum(sum_src, (int)n_src);
  /* sum_{0..63} i^2 = 85344; + 64*7 = 448 -> 85792 */
  if (g_sum64 != 85792)
    return 0xE2000064;

  /* countdown (unconverted shape): values n..1 */
  n_src = 9;
  countdown(cd_buf, n_src);
  for (unsigned i = 0; i < 9; i++)
    if (cd_buf[i] != 9 - i)
      return 0xE3000000 | i;
  if (cd_buf[9] != 0x5A5A5A5A)
    return 0xE300005A;

  return 0xCAFEBABE;
}
