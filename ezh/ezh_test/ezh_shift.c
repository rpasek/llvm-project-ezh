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

// ALU-Shift and Shift-ALU Test Functions
__attribute__((noinline)) uint32_t test_add_lsl(uint32_t a, uint32_t b) {
  return (a + b) << 2;
}
__attribute__((noinline)) uint32_t test_sub_lsr(uint32_t a, uint32_t b) {
  return (a - b) >> 3;
}
__attribute__((noinline)) int test_and_asr(int a, int b) {
  return (a & b) >> 4;
}
__attribute__((noinline)) uint32_t test_or_ror(uint32_t a, uint32_t b) {
  uint32_t val = a | b;
  return (val >> 5) | (val << 27);
}
__attribute__((noinline)) uint32_t test_addn_lsl(uint32_t a, uint32_t b) {
  return ~(a + b) << 2;
}
__attribute__((noinline)) uint32_t test_subn_lsr(uint32_t a, uint32_t b) {
  return ~(a - b) >> 3;
}

__attribute__((noinline)) uint32_t test_rlsl_add(uint32_t a, uint32_t b,
                                                 uint32_t c) {
  return a + (b << c);
}
__attribute__((noinline)) uint32_t test_rlsr_and(uint32_t a, uint32_t b,
                                                 uint32_t c) {
  return a & (b >> c);
}
__attribute__((noinline)) int test_rasr_or(int a, int b, uint32_t c) {
  return a | (b >> c);
}
__attribute__((noinline)) uint32_t test_rror_xor(uint32_t a, uint32_t b,
                                                 uint32_t c) {
  uint32_t shifted = (b >> c) | (b << (32 - c));
  return a ^ shifted;
}
__attribute__((noinline)) uint32_t test_rlsl_sub(uint32_t a, uint32_t b,
                                                 uint32_t c) {
  return (b << c) - a;
}

__attribute__((noinline)) uint32_t test_add_rlsl(uint32_t a, uint32_t b,
                                                 uint32_t c) {
  return (a + b) << c;
}
__attribute__((noinline)) uint32_t test_and_rlsr(uint32_t a, uint32_t b,
                                                 uint32_t c) {
  return (a & b) >> c;
}
__attribute__((noinline)) int test_or_rasr(int a, int b, uint32_t c) {
  return (a | b) >> c;
}
__attribute__((noinline)) uint32_t test_xor_rror(uint32_t a, uint32_t b,
                                                 uint32_t c) {
  uint32_t val = a ^ b;
  return (val >> c) | (val << (32 - c));
}
__attribute__((noinline)) uint32_t test_sub_rlsl(uint32_t a, uint32_t b,
                                                 uint32_t c) {
  return (b - a) << c;
}

__attribute__((noinline)) uint32_t test_rlsl(uint32_t a, uint32_t b) {
  return a << b;
}
__attribute__((noinline)) uint32_t test_rlsr(uint32_t a, uint32_t b) {
  return a >> b;
}
__attribute__((noinline)) int test_rasr(int a, uint32_t b) { return a >> b; }
__attribute__((noinline)) uint32_t test_rror(uint32_t a, uint32_t b) {
  return (a >> b) | (a << (32 - b));
}

__attribute__((noinline, optnone)) uint32_t hardware_sub_lsr(uint32_t a,
                                                             uint32_t b) {
  __asm__ volatile("sub_lsr %0, %0, %1, 8" : "+r"(a) : "r"(b));
  return a;
}

__attribute__((noinline, optnone)) void test_shifted_subs() {
  volatile int a = 100;
  volatile int b = 6;
  __asm__ volatile("" : "+r"(a));
  __asm__ volatile("" : "+r"(b));

  TEST_COND(int, a, (b << 1), -, 88);
  TEST_COND(int, a, (b >> 1), -, 97);
  TEST_COND(int, a, (b >> 1), -, 97);

  volatile int ror_val = (b >> 1) | (b << 31);
  __asm__ volatile("" : "+r"(ror_val));
  TEST_COND(int, a, ror_val, -, 97);
}

__attribute__((noinline, optnone)) void verify_alu_shift() {
  // ALU then Constant Shift
  exc_signal = __LINE__;
  if (test_add_lsl(10U, 20U) != ((10U + 20U) << 2)) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_sub_lsr(100U, 20U) != ((100U - 20U) >> 3)) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_and_asr(0xFF, 0x55) != ((0xFF & 0x55) >> 4)) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_or_ror(0x55U, 0xAAU) !=
      (((0x55U | 0xAAU) >> 5) | ((0x55U | 0xAAU) << 27))) {
    DEAD();
  }

  // ALU then Constant Shift (Inverted)
  exc_signal = __LINE__;
  if (test_addn_lsl(10U, 20U) != (~(10U + 20U) << 2)) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_subn_lsr(100U, 20U) != (~(100U - 20U) >> 3)) {
    DEAD();
  }

  // Register Shift then ALU
  exc_signal = __LINE__;
  if (test_rlsl_add(10U, 20U, 2) != (10U + (20U << 2))) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_rlsr_and(0xFFU, 0x55U, 3) != (0xFFU & (0x55U >> 3))) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_rasr_or(-100, 50, 4) != (-100 | (50 >> 4))) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_rror_xor(0x55U, 0xAAU, 5) !=
      (0x55U ^ ((0xAAU >> 5) | (0xAAU << 27)))) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_rlsl_sub(10U, 20U, 2) != ((20U << 2) - 10U)) {
    DEAD();
  }

  // ALU then Register Shift
  exc_signal = __LINE__;
  if (test_add_rlsl(10U, 20U, 2) != ((10U + 20U) << 2)) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_and_rlsr(0xFFU, 0x55U, 3) != ((0xFFU & 0x55U) >> 3)) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_or_rasr(-100, 50, 4) != ((-100 | 50) >> 4)) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_xor_rror(0x55U, 0xAAU, 5) !=
      (((0x55U ^ 0xAAU) >> 5) | ((0x55U ^ 0xAAU) << 27))) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_sub_rlsl(100U, 20U, 2) != ((20U - 100U) << 2)) {
    DEAD();
  }

  // Basic Register Shifts
  exc_signal = __LINE__;
  if (test_rlsl(10U, 2) != (10U << 2)) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_rlsr(100U, 3) != (100U >> 3)) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_rasr(-100, 4) != (-100 >> 4)) {
    DEAD();
  }
  exc_signal = __LINE__;
  if (test_rror(0xAAU, 5) != (((0xAAU >> 5) | (0xAAU << 27)))) {
    DEAD();
  }
}

int main() {
  exc_signal = 0xCAFE0001;

  test_shifted_subs();
  verify_alu_shift();

  return 0xCAFEBABE;
}
