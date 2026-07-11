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

__attribute__((noinline)) int test_addn(int a, int b) { return ~(a + b); }
__attribute__((noinline)) int test_andn(int a, int b) { return ~(a & b); }
__attribute__((noinline)) int test_orn(int a, int b) { return ~(a | b); }
__attribute__((noinline)) int test_xorn(int a, int b) { return ~(a ^ b); }
__attribute__((noinline)) int test_andor(int a, int b, int c) {
  return (a & b) | c;
}
__attribute__((noinline)) int test_bset(int a, int bit) {
  return a | (1 << bit);
}
__attribute__((noinline)) int test_bclr(int a, int bit) {
  return a & ~(1 << bit);
}
__attribute__((noinline)) int test_btog(int a, int bit) {
  return a ^ (1 << bit);
}
__attribute__((noinline)) int test_btst(int a, int bit) {
  return (a >> bit) & 1;
}
__attribute__((noinline)) int test_bset_imm(int a) { return a | 0x4; }
__attribute__((noinline)) int test_bclr_imm(int a) { return a & ~0x8; }
__attribute__((noinline)) int test_btog_imm(int a) { return a ^ 0x10; }

__attribute__((noinline)) uint32_t test_bswap(uint32_t a) {
  return __builtin_bswap32(a);
}
__attribute__((noinline)) uint32_t test_bitreverse(uint32_t a) {
  return __builtin_bitreverse32(a);
}

__attribute__((noinline, optnone)) void test_lowering() {
  // Negate ALU
  exc_signal = __LINE__;
  if (test_addn(10, 20) != ~(10 + 20)) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_andn(0xFF, 0xAA) != ~(0xFF & 0xAA)) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_orn(0x55, 0xAA) != ~(0x55 | 0xAA)) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_xorn(0x55, 0xAA) != ~(0x55 ^ 0xAA)) {
    DEAD();
  }

  // Combined AND-then-OR
  exc_signal = __LINE__;
  if (test_andor(0xFF, 0x55, 0xAA) != ((0xFF & 0x55) | 0xAA)) {
    DEAD();
  }

  // Bit Manipulation
  exc_signal = __LINE__;
  if (test_bset(0, 5) != 32) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_bclr(0xFF, 3) != 0xF7) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_btog(0, 4) != 16) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_btst(32, 5) != 1) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_btst(32, 4) != 0) {
    DEAD();
  }

  // Immediate Bit Manipulation
  exc_signal = __LINE__;
  if (test_bset_imm(0) != 4) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_bclr_imm(0xFF) != 0xF7) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_btog_imm(0) != 16) {
    DEAD();
  }

  // Flip Operations (bswap & bitreverse)
  exc_signal = __LINE__;
  if (test_bswap(0x12345678U) != 0x78563412U) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_bitreverse(0xF0F0F0F0U) != 0x0F0F0F0FU) {
    DEAD();
  }
}

int main() {
  exc_signal = 0xCAFE0001;

  test_lowering();

  return 0xCAFEBABE;
}
