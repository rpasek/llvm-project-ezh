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

__attribute__((noinline)) void test_8bit_array() {
  exc_signal = __LINE__;
  volatile uint8_t *ptr8 = (volatile uint8_t *)0x004F8030;
  ptr8[0] = 0xAA;
  ptr8[1] = 0xBB;
  ptr8[2] = 0xCC;
  ptr8[3] = 0xDD;
  if (ptr8[0] != 0xAA || ptr8[1] != 0xBB || ptr8[2] != 0xCC ||
      ptr8[3] != 0xDD) {
    DEAD();
  }
}

__attribute__((noinline)) void test_16bit_array() {
  exc_signal = __LINE__;
  volatile uint16_t array16[4];
  array16[0] = 0x1234;
  array16[1] = 0x5678;
  array16[2] = 0x9ABC;
  array16[3] = 0xDEF0;
  if (array16[0] != 0x1234 || array16[1] != 0x5678 || array16[2] != 0x9ABC ||
      array16[3] != 0xDEF0) {
    DEAD();
  }
}

__attribute__((noinline)) void test_32bit_array() {
  exc_signal = __LINE__;
  volatile uint32_t array32[4];
  array32[0] = 0x11111111;
  array32[1] = 0x22222222;
  array32[2] = 0x33333333;
  array32[3] = 0x44444444;
  if (array32[0] != 0x11111111 || array32[1] != 0x22222222 ||
      array32[2] != 0x33333333 || array32[3] != 0x44444444) {
    DEAD();
  }
}

__attribute__((noinline)) void test_64bit_array() {
  exc_signal = __LINE__;
  volatile uint64_t array64[4];
  array64[0] = 0x1122334455667788ULL;
  array64[1] = 0x2233445566778899ULL;
  array64[2] = 0x33445566778899AAULL;
  array64[3] = 0x445566778899AABBULL;
  if (array64[0] != 0x1122334455667788ULL ||
      array64[1] != 0x2233445566778899ULL ||
      array64[2] != 0x33445566778899AAULL ||
      array64[3] != 0x445566778899AABBULL) {
    DEAD();
  }
}

int main() {
  exc_signal = 0xCAFE0001;
  test_8bit_array();
  test_16bit_array();
  test_32bit_array();
  test_64bit_array();
  return 0xCAFEBABE;
}
