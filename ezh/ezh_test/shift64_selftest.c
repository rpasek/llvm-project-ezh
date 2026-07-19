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
 * On-board self-test for the inline variable 64-bit shift expansion
 * (SHL/SRL/SRA_PARTS): the regression suite builds at -O0/-Os where the
 * shapes are rare, so this pins the -O2 parts sequence on silicon across
 * the word boundary. Exits 0 (exc_signal 0xCAFEBABE) iff every checked
 * shift matches the reference. Runner: run_shift64.sh.
 */

volatile unsigned r_fails = 0;
volatile unsigned r_cases = 0;

/* noinline + volatile amount so the variable-shift path is forced */
static unsigned long long __attribute__((noinline)) do_shl(unsigned long long v, int n) { return v << n; }
static unsigned long long __attribute__((noinline)) do_lshr(unsigned long long v, int n) { return v >> n; }
static long long __attribute__((noinline)) do_ashr(long long v, int n) { return v >> n; }

volatile int amounts[8] = {0, 1, 5, 31, 32, 33, 47, 63};
volatile unsigned long long val = 0x8899AABBCCDDEEFFULL;

int main(void) {
  unsigned long long v = val;
  for (int i = 0; i < 8; i++) {
    int n = amounts[i];
    /* references computed with constant shifts via a switch, which never
     * takes the parts path (the type legalizer resolves constants) */
    unsigned long long eshl, elshr;
    long long eashr;
    switch (n) {
    case 0:  eshl = v << 0;  elshr = v >> 0;  eashr = (long long)v >> 0;  break;
    case 1:  eshl = v << 1;  elshr = v >> 1;  eashr = (long long)v >> 1;  break;
    case 5:  eshl = v << 5;  elshr = v >> 5;  eashr = (long long)v >> 5;  break;
    case 31: eshl = v << 31; elshr = v >> 31; eashr = (long long)v >> 31; break;
    case 32: eshl = v << 32; elshr = v >> 32; eashr = (long long)v >> 32; break;
    case 33: eshl = v << 33; elshr = v >> 33; eashr = (long long)v >> 33; break;
    case 47: eshl = v << 47; elshr = v >> 47; eashr = (long long)v >> 47; break;
    default: eshl = v << 63; elshr = v >> 63; eashr = (long long)v >> 63; break;
    }
    if (do_shl(v, n) != eshl) r_fails++;
    if (do_lshr(v, n) != elshr) r_fails++;
    if (do_ashr((long long)v, n) != eashr) r_fails++;
    r_cases += 3;
  }
  return r_fails == 0 ? 0 : 1;
}
