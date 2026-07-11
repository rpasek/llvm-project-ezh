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

#ifdef __cplusplus
extern "C" {
#endif
extern volatile int exc_signal;
extern void exit(int status);
#ifdef __cplusplus
}
#endif

#define DEAD() exit(0xDEAD0000 | (__LINE__ & 0xFFFF))

#define TEST_COND(type, a, b, op, expected)                                    \
  do {                                                                         \
    exc_signal = __LINE__;                                                     \
    volatile type va = a;                                                      \
    volatile type vb = b;                                                      \
    __asm__ volatile("" : "+r"(va));                                           \
    __asm__ volatile("" : "+r"(vb));                                           \
    volatile type res = (va op vb);                                            \
    if (res != expected) {                                                     \
      DEAD();                                                                  \
    }                                                                          \
  } while (0)

#endif // EZH_TEST_H
