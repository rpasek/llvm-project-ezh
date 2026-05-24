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

#include "ezh_test.h"
#include <stdio.h>

static __attribute__((noinline)) int test_addn(int a, int b) {
  return ~(a + b);
}
static __attribute__((noinline)) int test_andn(int a, int b) {
  return ~(a & b);
}
static __attribute__((noinline)) int test_orn(int a, int b) { return ~(a | b); }
static __attribute__((noinline)) int test_xorn(int a, int b) {
  return ~(a ^ b);
}
static __attribute__((noinline)) int test_andor(int a, int b, int c) {
  return (a & b) | c;
}
static __attribute__((noinline)) int test_bset(int a, int bit) {
  return a | (1 << bit);
}
static __attribute__((noinline)) int test_bclr(int a, int bit) {
  return a & ~(1 << bit);
}
static __attribute__((noinline)) int test_btog(int a, int bit) {
  return a ^ (1 << bit);
}
static __attribute__((noinline)) int test_btst(int a, int bit) {
  return (a >> bit) & 1;
}
static __attribute__((noinline)) int test_bset_imm(int a) { return a | 0x4; }
static __attribute__((noinline)) int test_bclr_imm(int a) { return a & ~0x8; }
static __attribute__((noinline)) int test_btog_imm(int a) { return a ^ 0x10; }

static __attribute__((noinline)) uint32_t test_bswap(uint32_t a) {
  return __builtin_bswap32(a);
}
static __attribute__((noinline)) uint32_t test_bitreverse(uint32_t a) {
  return __builtin_bitreverse32(a);
}

static __attribute__((noinline, optnone)) void test_lowering() {
  // 1. Negate ALU (except subn which is not optimized yet)
  exc_signal = 0x8001;
  if (test_addn(10, 20) != ~(10 + 20)) {
    while (1)
      ;
  }
  exc_signal = 0x8002;
  if (test_andn(0xFF, 0xAA) != ~(0xFF & 0xAA)) {
    while (1)
      ;
  }
  exc_signal = 0x8003;
  if (test_orn(0x55, 0xAA) != ~(0x55 | 0xAA)) {
    while (1)
      ;
  }
  exc_signal = 0x8004;
  if (test_xorn(0x55, 0xAA) != ~(0x55 ^ 0xAA)) {
    while (1)
      ;
  }

  // 2. Combined AND-then-OR
  exc_signal = 0x8005;
  if (test_andor(0xFF, 0x55, 0xAA) != ((0xFF & 0x55) | 0xAA)) {
    while (1)
      ;
  }

  // 3. Dynamic Bit Manipulation
  exc_signal = 0x8006;
  if (test_bset(0, 5) != 32) {
    while (1)
      ;
  }
  exc_signal = 0x8007;
  if (test_bclr(0xFF, 3) != 0xF7) {
    while (1)
      ;
  }
  exc_signal = 0x8008;
  if (test_btog(0, 4) != 16) {
    while (1)
      ;
  }
  exc_signal = 0x8009;
  if (test_btst(32, 5) != 1) {
    while (1)
      ;
  }
  exc_signal = 0x800A;
  if (test_btst(32, 4) != 0) {
    while (1)
      ;
  }

  // 4. Immediate Bit Manipulation
  exc_signal = 0x800B;
  if (test_bset_imm(0) != 4) {
    while (1)
      ;
  }
  exc_signal = 0x800C;
  if (test_bclr_imm(0xFF) != 0xF7) {
    while (1)
      ;
  }
  exc_signal = 0x800D;
  if (test_btog_imm(0) != 16) {
    while (1)
      ;
  }

  // 5. Flip Operations (bswap & bitreverse)
  exc_signal = 0xB001;
  if (test_bswap(0x12345678U) != 0x78563412U) {
    while (1)
      ;
  }
  exc_signal = 0xB002;
  if (test_bitreverse(0xF0F0F0F0U) != 0x0F0F0F0FU) {
    while (1)
      ;
  }
}

int main() {
  exc_signal = 0x11111111;

  test_lowering();

  exc_signal = (int)0xCAFEBABE;
  return (int)0xCAFEBABE;
}
