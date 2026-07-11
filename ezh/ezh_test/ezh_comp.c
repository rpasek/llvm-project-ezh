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

__attribute__((noinline, optnone)) void test_cond_i8() {
  TEST_COND(int8_t, 10, 20, ==, 0);
  TEST_COND(int8_t, 20, 20, ==, 1);
  TEST_COND(int8_t, 10, 20, !=, 1);
  TEST_COND(int8_t, 20, 20, !=, 0);
  TEST_COND(int8_t, -10, 10, <, 1);
  TEST_COND(int8_t, 10, 10, <, 0);
  TEST_COND(int8_t, -10, 10, <=, 1);
  TEST_COND(int8_t, 10, 10, <=, 1);
  TEST_COND(int8_t, 10, -10, >, 1);
  TEST_COND(int8_t, 10, 10, >, 0);
  TEST_COND(int8_t, 10, -10, >=, 1);
  TEST_COND(int8_t, 10, 10, >=, 1);
  TEST_COND(int8_t, -128, 127, <, 1);
  TEST_COND(int8_t, 127, -128, >, 1);
}

__attribute__((noinline, optnone)) void test_cond_u8() {
  TEST_COND(uint8_t, 10, 20, ==, 0);
  TEST_COND(uint8_t, 20, 20, ==, 1);
  TEST_COND(uint8_t, 10, 20, !=, 1);
  TEST_COND(uint8_t, 20, 20, !=, 0);
  TEST_COND(uint8_t, 10, 20, <, 1);
  TEST_COND(uint8_t, 20, 20, <, 0);
  TEST_COND(uint8_t, 10, 20, <=, 1);
  TEST_COND(uint8_t, 20, 20, <=, 1);
  TEST_COND(uint8_t, 20, 10, >, 1);
  TEST_COND(uint8_t, 20, 20, >, 0);
  TEST_COND(uint8_t, 20, 10, >=, 1);
  TEST_COND(uint8_t, 20, 20, >=, 1);
  TEST_COND(uint8_t, 0, 255, <, 1);
  TEST_COND(uint8_t, 255, 0, >, 1);
}

__attribute__((noinline, optnone)) void test_cond_i16() {
  TEST_COND(int16_t, 10, 20, ==, 0);
  TEST_COND(int16_t, 20, 20, ==, 1);
  TEST_COND(int16_t, 10, 20, !=, 1);
  TEST_COND(int16_t, 20, 20, !=, 0);
  TEST_COND(int16_t, -10, 10, <, 1);
  TEST_COND(int16_t, 10, 10, <, 0);
  TEST_COND(int16_t, -10, 10, <=, 1);
  TEST_COND(int16_t, 10, 10, <=, 1);
  TEST_COND(int16_t, 10, -10, >, 1);
  TEST_COND(int16_t, 10, 10, >, 0);
  TEST_COND(int16_t, 10, -10, >=, 1);
  TEST_COND(int16_t, 10, 10, >=, 1);
  TEST_COND(int16_t, -32768, 32767, <, 1);
  TEST_COND(int16_t, 32767, -32768, >, 1);
}

__attribute__((noinline, optnone)) void test_cond_u16() {
  TEST_COND(uint16_t, 10, 20, ==, 0);
  TEST_COND(uint16_t, 20, 20, ==, 1);
  TEST_COND(uint16_t, 10, 20, !=, 1);
  TEST_COND(uint16_t, 20, 20, !=, 0);
  TEST_COND(uint16_t, 10, 20, <, 1);
  TEST_COND(uint16_t, 20, 20, <, 0);
  TEST_COND(uint16_t, 10, 20, <=, 1);
  TEST_COND(uint16_t, 20, 20, <=, 1);
  TEST_COND(uint16_t, 20, 10, >, 1);
  TEST_COND(uint16_t, 20, 20, >, 0);
  TEST_COND(uint16_t, 20, 10, >=, 1);
  TEST_COND(uint16_t, 20, 20, >=, 1);
  TEST_COND(uint16_t, 0, 65535, <, 1);
  TEST_COND(uint16_t, 65535, 0, >, 1);
}

__attribute__((noinline, optnone)) void test_cond_i32() {
  TEST_COND(int32_t, 10, 20, ==, 0);
  TEST_COND(int32_t, 20, 20, ==, 1);
  TEST_COND(int32_t, 10, 20, !=, 1);
  TEST_COND(int32_t, 20, 20, !=, 0);
  TEST_COND(int32_t, -10, 10, <, 1);
  TEST_COND(int32_t, 10, 10, <, 0);
  TEST_COND(int32_t, -10, 10, <=, 1);
  TEST_COND(int32_t, 10, 10, <=, 1);
  TEST_COND(int32_t, 10, -10, >, 1);
  TEST_COND(int32_t, 10, 10, >, 0);
  TEST_COND(int32_t, 10, -10, >=, 1);
  TEST_COND(int32_t, 10, 10, >=, 1);
  TEST_COND(int32_t, -2147483648LL, 2147483647LL, <, 1);
  TEST_COND(int32_t, 2147483647LL, -2147483648LL, >, 1);
}

__attribute__((noinline, optnone)) void test_cond_u32() {
  TEST_COND(uint32_t, 10, 20, ==, 0);
  TEST_COND(uint32_t, 20, 20, ==, 1);
  TEST_COND(uint32_t, 10, 20, !=, 1);
  TEST_COND(uint32_t, 20, 20, !=, 0);
  TEST_COND(uint32_t, 10, 20, <, 1);
  TEST_COND(uint32_t, 20, 20, <, 0);
  TEST_COND(uint32_t, 10, 20, <=, 1);
  TEST_COND(uint32_t, 20, 20, <=, 1);
  TEST_COND(uint32_t, 20, 10, >, 1);
  TEST_COND(uint32_t, 20, 20, >, 0);
  TEST_COND(uint32_t, 20, 10, >=, 1);
  TEST_COND(uint32_t, 20, 20, >=, 1);
  TEST_COND(uint32_t, 0, 4294967295U, <, 1);
  TEST_COND(uint32_t, 4294967295U, 0, >, 1);
}

int main() {
  exc_signal = 0xCAFE0001;
  test_cond_i8();
  exc_signal = __LINE__;
  test_cond_u8();
  exc_signal = __LINE__;
  test_cond_i16();
  exc_signal = __LINE__;
  test_cond_u16();
  exc_signal = __LINE__;
  test_cond_i32();
  exc_signal = __LINE__;
  test_cond_u32();
  return 0xCAFEBABE;
}
