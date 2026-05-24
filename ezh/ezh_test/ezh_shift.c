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
static __attribute__((noinline)) uint32_t test_add_lsl(uint32_t a, uint32_t b) {
  return (a + b) << 2;
}
static __attribute__((noinline)) uint32_t test_sub_lsr(uint32_t a, uint32_t b) {
  return (a - b) >> 3;
}
static __attribute__((noinline)) int test_and_asr(int a, int b) {
  return (a & b) >> 4;
}
static __attribute__((noinline)) uint32_t test_or_ror(uint32_t a, uint32_t b) {
  uint32_t val = a | b;
  return (val >> 5) | (val << 27);
}
static __attribute__((noinline)) uint32_t test_addn_lsl(uint32_t a,
                                                        uint32_t b) {
  return ~(a + b) << 2;
}
static __attribute__((noinline)) uint32_t test_subn_lsr(uint32_t a,
                                                        uint32_t b) {
  return ~(a - b) >> 3;
}

static __attribute__((noinline)) uint32_t test_rlsl_add(uint32_t a, uint32_t b,
                                                        uint32_t c) {
  return a + (b << c);
}
static __attribute__((noinline)) uint32_t test_rlsr_and(uint32_t a, uint32_t b,
                                                        uint32_t c) {
  return a & (b >> c);
}
static __attribute__((noinline)) int test_rasr_or(int a, int b, uint32_t c) {
  return a | (b >> c);
}
static __attribute__((noinline)) uint32_t test_rror_xor(uint32_t a, uint32_t b,
                                                        uint32_t c) {
  uint32_t shifted = (b >> c) | (b << (32 - c));
  return a ^ shifted;
}
static __attribute__((noinline)) uint32_t test_rlsl_sub(uint32_t a, uint32_t b,
                                                        uint32_t c) {
  return (b << c) - a;
}

static __attribute__((noinline)) uint32_t test_add_rlsl(uint32_t a, uint32_t b,
                                                        uint32_t c) {
  return (a + b) << c;
}
static __attribute__((noinline)) uint32_t test_and_rlsr(uint32_t a, uint32_t b,
                                                        uint32_t c) {
  return (a & b) >> c;
}
static __attribute__((noinline)) int test_or_rasr(int a, int b, uint32_t c) {
  return (a | b) >> c;
}
static __attribute__((noinline)) uint32_t test_xor_rror(uint32_t a, uint32_t b,
                                                        uint32_t c) {
  uint32_t val = a ^ b;
  return (val >> c) | (val << (32 - c));
}
static __attribute__((noinline)) uint32_t test_sub_rlsl(uint32_t a, uint32_t b,
                                                        uint32_t c) {
  return (b - a) << c;
}

static __attribute__((noinline)) uint32_t test_rlsl(uint32_t a, uint32_t b) {
  return a << b;
}
static __attribute__((noinline)) uint32_t test_rlsr(uint32_t a, uint32_t b) {
  return a >> b;
}
static __attribute__((noinline)) int test_rasr(int a, uint32_t b) {
  return a >> b;
}
static __attribute__((noinline)) uint32_t test_rror(uint32_t a, uint32_t b) {
  return (a >> b) | (a << (32 - b));
}

static __attribute__((noinline, optnone)) uint32_t
hardware_sub_lsr(uint32_t a, uint32_t b) {
  __asm__ volatile("sub_lsr %0, %0, %1, 8" : "+r"(a) : "r"(b));
  return a;
}

static __attribute__((noinline, optnone)) void test_shifted_subs() {
  volatile int a = 100;
  volatile int b = 6;
  __asm__ volatile("" : "+r"(a));
  __asm__ volatile("" : "+r"(b));

  TEST_COND(int, a, (b << 1), -, 88, 0x8B01);
  TEST_COND(int, a, (b >> 1), -, 97, 0x8B02);
  TEST_COND(int, a, (b >> 1), -, 97, 0x8B03);

  volatile int ror_val = (b >> 1) | (b << 31);
  __asm__ volatile("" : "+r"(ror_val));
  TEST_COND(int, a, ror_val, -, 97, 0x8B04);
}

static __attribute__((noinline, optnone)) void verify_alu_shift() {
  // 1. ALU then Constant Shift
  exc_signal = 0xA001;
  if (test_add_lsl(10U, 20U) != ((10U + 20U) << 2)) {
    while (1)
      ;
  }
  exc_signal = 0xA002;
  if (test_sub_lsr(100U, 20U) != ((100U - 20U) >> 3)) {
    while (1)
      ;
  }
  exc_signal = 0xA003;
  if (test_and_asr(0xFF, 0x55) != ((0xFF & 0x55) >> 4)) {
    while (1)
      ;
  }
  exc_signal = 0xA004;
  if (test_or_ror(0x55U, 0xAAU) !=
      (((0x55U | 0xAAU) >> 5) | ((0x55U | 0xAAU) << 27))) {
    while (1)
      ;
  }

  // 2. ALU then Constant Shift (Inverted)
  exc_signal = 0xA005;
  if (test_addn_lsl(10U, 20U) != (~(10U + 20U) << 2)) {
    while (1)
      ;
  }
  exc_signal = 0xA006;
  if (test_subn_lsr(100U, 20U) != (~(100U - 20U) >> 3)) {
    while (1)
      ;
  }

  // 3. Register Shift then ALU
  exc_signal = 0xA007;
  if (test_rlsl_add(10U, 20U, 2) != (10U + (20U << 2))) {
    while (1)
      ;
  }
  exc_signal = 0xA008;
  if (test_rlsr_and(0xFFU, 0x55U, 3) != (0xFFU & (0x55U >> 3))) {
    while (1)
      ;
  }
  exc_signal = 0xA009;
  if (test_rasr_or(-100, 50, 4) != (-100 | (50 >> 4))) {
    while (1)
      ;
  }
  exc_signal = 0xA00A;
  if (test_rror_xor(0x55U, 0xAAU, 5) !=
      (0x55U ^ ((0xAAU >> 5) | (0xAAU << 27)))) {
    while (1)
      ;
  }
  exc_signal = 0xA00B;
  if (test_rlsl_sub(10U, 20U, 2) != ((20U << 2) - 10U)) {
    while (1)
      ;
  }

  // 4. ALU then Register Shift
  exc_signal = 0xA00C;
  if (test_add_rlsl(10U, 20U, 2) != ((10U + 20U) << 2)) {
    while (1)
      ;
  }
  exc_signal = 0xA00D;
  if (test_and_rlsr(0xFFU, 0x55U, 3) != ((0xFFU & 0x55U) >> 3)) {
    while (1)
      ;
  }
  exc_signal = 0xA00E;
  if (test_or_rasr(-100, 50, 4) != ((-100 | 50) >> 4)) {
    while (1)
      ;
  }
  exc_signal = 0xA00F;
  if (test_xor_rror(0x55U, 0xAAU, 5) !=
      (((0x55U ^ 0xAAU) >> 5) | ((0x55U ^ 0xAAU) << 27))) {
    while (1)
      ;
  }
  exc_signal = 0xA010;
  if (test_sub_rlsl(100U, 20U, 2) != ((20U - 100U) << 2)) {
    while (1)
      ;
  }

  // 5. Basic Register Shifts
  exc_signal = 0xA011;
  if (test_rlsl(10U, 2) != (10U << 2)) {
    while (1)
      ;
  }
  exc_signal = 0xA012;
  if (test_rlsr(100U, 3) != (100U >> 3)) {
    while (1)
      ;
  }
  exc_signal = 0xA013;
  if (test_rasr(-100, 4) != (-100 >> 4)) {
    while (1)
      ;
  }
  exc_signal = 0xA014;
  if (test_rror(0xAAU, 5) != (((0xAAU >> 5) | (0xAAU << 27)))) {
    while (1)
      ;
  }

  // 6. Direct Hardware E_SUB_LSR Test with detailed printfs
  exc_signal = 0xA0F0;
  uint32_t res1 = hardware_sub_lsr(0x24100530, 0x24100534);
  uint32_t res2 = hardware_sub_lsr(0x24100518, 0x24100534);
  uint32_t res3 = hardware_sub_lsr(0x24100538, 0x24100534);

  printf("E_SUB_LSR(0x24100530, 0x24100534, 8) = 0x%08x\n", res1);
  printf("E_SUB_LSR(0x24100518, 0x24100534, 8) = 0x%08x\n", res2);
  printf("E_SUB_LSR(0x24100538, 0x24100534, 8) = 0x%08x\n", res3);
}

int main() {
  exc_signal = 0x11111111;

  test_shifted_subs();
  verify_alu_shift();

  exc_signal = (int)0xCAFEBABE;
  return (int)0xCAFEBABE;
}
