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
#include <stdarg.h>
#include <stdio.h>

__attribute__((noinline)) int func5(int x) {
  volatile uint32_t arr[4];
  for (int i = 0; i < 4; i++)
    arr[i] = 0x55550000 + i;
  return x + 5;
}

__attribute__((noinline)) int func4(int x) {
  volatile uint32_t arr[4];
  for (int i = 0; i < 4; i++)
    arr[i] = 0x44440000U + i;
  int r = func5(x + 1);
  exc_signal = __LINE__;
  for (int i = 0; i < 4; i++) {
    if (arr[i] != 0x44440000U + i) {
      DEAD();
    }
  }
  return r;
}

__attribute__((noinline)) int func3(int x) {
  volatile uint32_t arr[4];
  for (int i = 0; i < 4; i++)
    arr[i] = 0x33330000U + i;
  int r = func4(x + 1);
  exc_signal = __LINE__;
  for (int i = 0; i < 4; i++) {
    if (arr[i] != 0x33330000U + i) {
      DEAD();
    }
  }
  return r;
}

__attribute__((noinline)) int func2(int x) {
  volatile uint32_t arr[4];
  for (int i = 0; i < 4; i++)
    arr[i] = 0x22220000U + i;
  int r = func3(x + 1);
  exc_signal = __LINE__;
  for (int i = 0; i < 4; i++) {
    if (arr[i] != 0x22220000U + i) {
      DEAD();
    }
  }
  return r;
}

__attribute__((noinline)) int func1(int x) {
  volatile uint32_t arr[4];
  for (int i = 0; i < 4; i++)
    arr[i] = 0x11110000U + i;
  int r = func2(x + 1);
  exc_signal = __LINE__;
  for (int i = 0; i < 4; i++) {
    if (arr[i] != 0x11110000U + i) {
      DEAD();
    }
  }
  return r;
}

__attribute__((noinline, optnone)) void test_stack(int depth) {
  volatile uint32_t arr[4];

  for (int i = 0; i < 4; i++) {
    arr[i] = 0xAAAA0000 + depth + i;
  }
  if (depth > 0) {
    test_stack(depth - 1);
  }
  exc_signal = __LINE__;
  for (int i = 0; i < 4; i++) {
    if (arr[i] != 0xAAAA0000 + depth + i) {
      DEAD();
    }
  }
}

__attribute__((noinline)) void test_heap_ops() {
  extern void *malloc(unsigned long size);
  extern void free(void *ptr);
  extern char _end[];

  exc_signal = __LINE__;
  uint32_t *heap_ptr = (uint32_t *)malloc(16);
  if ((uint32_t)heap_ptr < (uint32_t)_end) {
    DEAD();
  }
  for (int i = 0; i < 4; i++) {
    heap_ptr[i] = 0xFEED0000 + i;
  }
  exc_signal = __LINE__;
  for (int i = 0; i < 4; i++) {
    if (heap_ptr[i] != 0xFEED0000 + i) {
      DEAD();
    }
  }
  free(heap_ptr);
  exc_signal = __LINE__;
}

__attribute__((noinline, optnone)) int small_switch(int x) {
  switch (x) {
  case 1:
    return 10;
  case 2:
    return 20;
  default:
    return 30;
  }
}

__attribute__((noinline, optnone)) int large_switch(int x) {
  switch (x) {
  case 1:
    return 10;
  case 2:
    return 20;
  case 3:
    return 30;
  case 4:
    return 40;
  case 5:
    return 50;
  case 6:
    return 60;
  default:
    return 70;
  }
}

__attribute__((noinline, optnone)) int test_for(int n) {
  int sum = 0;
  for (int i = 0; i < n; i++) {
    sum += i;
  }
  return sum;
}

__attribute__((noinline, optnone)) int test_while(int n) {
  int sum = 0;
  int i = 0;
  while (i < n) {
    sum += i;
    i++;
  }
  return sum;
}

__attribute__((noinline, optnone)) int test_do_while(int n) {
  int sum = 0;
  int i = 0;
  do {
    sum += i;
    i++;
  } while (i < n);
  return sum;
}

__attribute__((noinline, optnone)) int test_goto(int x) {
  if (x > 0)
    goto label_positive;
  return -1;
label_positive:
  return 1;
}

__attribute__((noinline)) int sum(int count, ...) {
  va_list args;
  va_start(args, count);
  int s = 0;
  for (int i = 0; i < count; i++) {
    s += va_arg(args, int);
  }
  va_end(args);
  return s;
}

__attribute__((noinline, optnone)) void test_varargs() {
  exc_signal = __LINE__; // Before varargs test
  int res = sum(3, 10, 20, 30);
  if (res != 60) {
    DEAD();
  }
}

__attribute__((noinline)) int test_post_dec_i32(int *ptr, int **out_ptr) {
  int val = *ptr--;
  *out_ptr = ptr;
  return val;
}

__attribute__((noinline)) int test_pre_dec_i32(int *ptr, int **out_ptr) {
  int val = *--ptr;
  *out_ptr = ptr;
  return val;
}

__attribute__((noinline)) uint8_t test_post_dec_i8(uint8_t *ptr,
                                                   uint8_t **out_ptr) {
  uint8_t val = *ptr--;
  *out_ptr = ptr;
  return val;
}

__attribute__((noinline)) uint8_t test_pre_dec_i8(uint8_t *ptr,
                                                  uint8_t **out_ptr) {
  uint8_t val = *--ptr;
  *out_ptr = ptr;
  return val;
}

int test_data_i32[3] = {100, 200, 300};
uint8_t test_data_i8[3] = {10, 20, 30};

__attribute__((noinline, optnone)) void verify_dec_modes() {
  int *ptr_i32;
  int *out_ptr_i32;
  uint8_t *ptr_i8;
  uint8_t *out_ptr_i8;

  // Test i32 post-dec
  ptr_i32 = &test_data_i32[2];
  exc_signal = __LINE__;
  if (test_post_dec_i32(ptr_i32, &out_ptr_i32) != 300) {
    DEAD();
  }
  if (out_ptr_i32 != &test_data_i32[1]) {
    DEAD();
  }

  // Test i32 pre-dec
  ptr_i32 = &test_data_i32[2];
  exc_signal = __LINE__;
  if (test_pre_dec_i32(ptr_i32, &out_ptr_i32) != 200) {
    DEAD();
  }
  if (out_ptr_i32 != &test_data_i32[1]) {
    DEAD();
  }

  // Test i8 post-dec
  ptr_i8 = &test_data_i8[2];
  exc_signal = __LINE__;
  if (test_post_dec_i8(ptr_i8, &out_ptr_i8) != 30) {
    DEAD();
  }
  if (out_ptr_i8 != &test_data_i8[1]) {
    DEAD();
  }

  // Test i8 pre-dec
  ptr_i8 = &test_data_i8[2];
  exc_signal = __LINE__;
  if (test_pre_dec_i8(ptr_i8, &out_ptr_i8) != 20) {
    DEAD();
  }
  if (out_ptr_i8 != &test_data_i8[1]) {
    DEAD();
  }
}

extern int bss_test_var;

__attribute__((noinline, optnone)) void test_large_frame() {
  exc_signal = __LINE__; // Large frame allocation error
  // 4500 bytes exceeds the 2040 bytes SUBri chunk allocation limit
  // and also forces constant pool materialization for offset mapping above
  // 2047/4096!
  volatile char stack_buffer[4500];
  stack_buffer[0] = 0x5A;
  stack_buffer[4499] = 0x25;

  if (stack_buffer[0] != 0x5A || stack_buffer[4499] != 0x25) {
    DEAD();
  }
}

__attribute__((noinline, optnone)) void test_stack_scavenge() {
  exc_signal = __LINE__; // Fallback off-by-4 address error
  // Large buffer to ensure out of range for i8 LDRB (offset > 127)
  volatile int8_t buffer[135];
  buffer[130] = 0x7E;

  // Clobber all available GPRs to force R6 push/pop fallback path
  __asm__ volatile("" : : : "r0", "r1", "r2", "r3", "r4", "r5");

  // Access out of range stack offset (130 > 127) under high register pressure
  if (buffer[130] != 0x7E) {
    DEAD();
  }
}

__attribute__((noinline)) int test_c_select(int cond, int a, int b) {
  return cond ? (a + b) : a;
}

static volatile int var_true = 0;
static volatile int var_false = 0;
static volatile int dummy_var = 0;

__attribute__((noinline)) void buggy_leaf_func() {
  if (dummy_var == 0) {
    var_true = 2;
  } else {
    var_false = 3;
  }
}

__attribute__((noinline, optnone)) void test_compiler_bug() {
  exc_signal = __LINE__;
  buggy_leaf_func();
  exc_signal = __LINE__;
}

__attribute__((noinline, optnone)) unsigned int test_unaligned_stack() {
  char buf[8] = {0};
  unsigned int result = 0;

  __asm__ volatile("add_imm r1, %1, 1\n\t"
                   "load_imm r2, 0x55\n\t"
                   "strb r1, r2, 0\n\t"
                   "ldrb r3, r1, 0\n\t"
                   "mov %0, r3\n\t"
                   : "=r"(result)
                   : "r"(buf)
                   : "r1", "r2", "r3");
  return result;
}

int main() {
  exc_signal = 0xCAFE0001;

  unsigned int unaligned_res = test_unaligned_stack();
  if (unaligned_res != 0x55) {
    exc_signal = 0xBAD00000 | unaligned_res;
    DEAD();
  }
  exc_signal = __LINE__;

  if (bss_test_var != 0) {
    DEAD();
  }

  test_large_frame();
  test_stack_scavenge();

  volatile int r = test_c_select(0, 10, 20);
  volatile int r2 = test_c_select(1, 10, 20);
  if (r != 10 || r2 != 30) {
    DEAD();
  }

  func1(0);
  test_stack(5);
  test_heap_ops();

  // New tests verification
  exc_signal = __LINE__; // Before small_switch
  if (small_switch(1) != 10 || small_switch(2) != 20 || small_switch(3) != 30) {
    DEAD();
  }

  exc_signal = __LINE__; // Before test_for
  if (test_for(5) != 10) {
    DEAD();
  }

  exc_signal = __LINE__; // Before test_while
  if (test_while(5) != 10) {
    DEAD();
  }

  exc_signal = __LINE__; // Before test_do_while
  if (test_do_while(5) != 10) {
    DEAD();
  }

  exc_signal = __LINE__; // Before test_goto
  if (test_goto(5) != 1 || test_goto(-5) != -1) {
    DEAD();
  }

  test_compiler_bug();
  test_varargs();
  verify_dec_modes();

  // Large switch might hang if buggy, so we do it last
  exc_signal = __LINE__; // Before large_switch
  if (large_switch(3) != 30 || large_switch(6) != 60 || large_switch(7) != 70) {
    DEAD();
  }

  return 0xCAFEBABE;
}
