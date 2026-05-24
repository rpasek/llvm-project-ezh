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

#ifndef EZH_TEST_H
#define EZH_TEST_H

#include <stdint.h>

extern volatile int exc_signal;
extern volatile uint32_t test_val;
extern volatile int wrong_result;

#define TEST_COND(type, a, b, op, expected, err_code)                          \
  do {                                                                         \
    exc_signal = err_code;                                                     \
    volatile type va = a;                                                      \
    volatile type vb = b;                                                      \
    __asm__ volatile("" : "+r"(va));                                           \
    __asm__ volatile("" : "+r"(vb));                                           \
    volatile type res = (va op vb);                                            \
    if (res != expected) {                                                     \
      while (1)                                                                \
        ;                                                                      \
    }                                                                          \
  } while (0)

#endif // EZH_TEST_H
