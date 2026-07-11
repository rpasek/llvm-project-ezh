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

__attribute__((noinline, optnone)) void test_ops_i8() {
  TEST_COND(int8_t, 100, 20, +, 120);
  TEST_COND(int8_t, 120, 100, -, 20);
  TEST_COND(int8_t, 12, 10, *, 120);
  TEST_COND(int8_t, 120, 10, /, 12);
  TEST_COND(int8_t, 123, 10, %, 3);
  TEST_COND(int8_t, 1, 3, <<, 8);
  TEST_COND(int8_t, 8, 2, >>, 2);
  TEST_COND(int8_t, 120, 7, +, 127);
  TEST_COND(int8_t, -120, 8, -, -128);
}

__attribute__((noinline, optnone)) void test_ops_u8() {
  TEST_COND(uint8_t, 200, 50, +, 250);
  TEST_COND(uint8_t, 250, 200, -, 50);
  TEST_COND(uint8_t, 25, 10, *, 250);
  TEST_COND(uint8_t, 250, 10, /, 25);
  TEST_COND(uint8_t, 253, 10, %, 3);
  TEST_COND(uint8_t, 1, 3, <<, 8);
  TEST_COND(uint8_t, 8, 2, >>, 2);
  TEST_COND(uint8_t, 250, 5, +, 255);
  TEST_COND(uint8_t, 250, 10, +, 4);
  TEST_COND(uint8_t, 5, 10, -, 251);
}

__attribute__((noinline, optnone)) void test_ops_i16() {
  TEST_COND(int16_t, 30000, 2000, +, 32000);
  TEST_COND(int16_t, 32000, 30000, -, 2000);
  TEST_COND(int16_t, 3200, 10, *, 32000);
  TEST_COND(int16_t, 32000, 10, /, 3200);
  TEST_COND(int16_t, 32003, 10, %, 3);
  TEST_COND(int16_t, 1, 3, <<, 8);
  TEST_COND(int16_t, 8, 2, >>, 2);
  TEST_COND(int16_t, 32760, 7, +, 32767);
}

__attribute__((noinline, optnone)) void test_ops_u16() {
  TEST_COND(uint16_t, 60000, 5000, +, 65000);
  TEST_COND(uint16_t, 65000, 60000, -, 5000);
  TEST_COND(uint16_t, 6500, 10, *, 65000);
  TEST_COND(uint16_t, 65000, 10, /, 6500);
  TEST_COND(uint16_t, 65003, 10, %, 3);
  TEST_COND(uint16_t, 1, 3, <<, 8);
  TEST_COND(uint16_t, 8, 2, >>, 2);
  TEST_COND(uint16_t, 65530, 5, +, 65535);
  TEST_COND(uint16_t, 65530, 10, +, 4);
  TEST_COND(uint16_t, 5, 10, -, 65531);
}

__attribute__((noinline, optnone)) void test_ops_i32() {
  TEST_COND(int32_t, 2000000000, 100000000, +, 2100000000);
  TEST_COND(int32_t, 2100000000, 2000000000, -, 100000000);
  TEST_COND(int32_t, 210000000, 10, *, 2100000000);
  TEST_COND(int32_t, 2100000000, 10, /, 210000000);
  TEST_COND(int32_t, 2100000003, 10, %, 3);
  TEST_COND(int32_t, 1, 3, <<, 8);
  TEST_COND(int32_t, 8, 2, >>, 2);
  TEST_COND(int32_t, 2147483640, 7, +, 2147483647);
}

__attribute__((noinline, optnone)) void test_ops_u32() {
  TEST_COND(uint32_t, 4000000000U, 200000000U, +, 4200000000U);
  TEST_COND(uint32_t, 4200000000U, 4000000000U, -, 200000000U);
  TEST_COND(uint32_t, 420000000U, 10U, *, 4200000000U);
  TEST_COND(uint32_t, 4200000000U, 10U, /, 420000000U);
  TEST_COND(uint32_t, 4200000003U, 10U, %, 3U);
  TEST_COND(uint32_t, 1, 3, <<, 8);
  TEST_COND(uint32_t, 8, 2, >>, 2);
  TEST_COND(uint32_t, 4294967290U, 5U, +, 4294967295U);
  TEST_COND(uint32_t, 4294967290U, 10U, +, 4U);
  TEST_COND(uint32_t, 5U, 10U, -, 4294967291U);
}

__attribute__((noinline, optnone)) void test_ops_i64() {
  TEST_COND(int64_t, 9000000000000000000LL, 200000000000000000LL, +,
            9200000000000000000LL);
  TEST_COND(int64_t, 9200000000000000000LL, 9000000000000000000LL, -,
            200000000000000000LL);
  TEST_COND(int64_t, 920000000000000000LL, 10LL, *, 9200000000000000000LL);
  TEST_COND(int64_t, 9200000000000000000LL, 10LL, /, 920000000000000000LL);
  TEST_COND(int64_t, 9200000000000000003LL, 10LL, %, 3LL);
  TEST_COND(int64_t, 1, 3, <<, 8);
  TEST_COND(int64_t, 8, 2, >>, 2);
  TEST_COND(int64_t, 9223372036854775800LL, 7LL, +, 9223372036854775807LL);
  TEST_COND(int64_t, 0x802001001LL, 10, >>, 0x2008004LL);
  TEST_COND(int64_t, 0x8000400100000080LL, 38, >>,
            (int64_t)0xFFFFFFFFFE000100ULL);
  TEST_COND(int64_t, 0x4000000058LL, 13, >>, 0x2000000LL);
  TEST_COND(int64_t, -0x4000000058LL, 13, >>, -0x2000001LL);
}

__attribute__((noinline, optnone)) void test_ops_u64() {
  TEST_COND(uint64_t, 18000000000000000000ULL, 400000000000000000ULL, +,
            18400000000000000000ULL);
  TEST_COND(uint64_t, 18400000000000000000ULL, 18000000000000000000ULL, -,
            400000000000000000ULL);
  TEST_COND(uint64_t, 1840000000000000000ULL, 10ULL, *,
            18400000000000000000ULL);
  TEST_COND(uint64_t, 18400000000000000000ULL, 10ULL, /,
            1840000000000000000ULL);
  TEST_COND(uint64_t, 18400000000000000003ULL, 10ULL, %, 3ULL);
  TEST_COND(uint64_t, 1, 3, <<, 8);
  TEST_COND(uint64_t, 8, 2, >>, 2);
  TEST_COND(uint64_t, 18446744073709551610ULL, 5ULL, +,
            18446744073709551615ULL);
  TEST_COND(uint64_t, 18446744073709551610ULL, 10ULL, +, 4ULL);
  TEST_COND(uint64_t, 5ULL, 10ULL, -, 18446744073709551611ULL);
  TEST_COND(uint64_t, 0x802001001ULL, 10, >>, 0x2008004ULL);
  TEST_COND(uint64_t, 0x8000400100000080ULL, 38, >>, 0x2000100ULL);
  TEST_COND(uint64_t, 0x4000000058ULL, 13, >>, 0x2000000ULL);
}

int main() {
  exc_signal = 0xCAFE0001;
  test_ops_i8();
  test_ops_u8();
  test_ops_i16();
  test_ops_u16();
  test_ops_i32();
  test_ops_u32();
  test_ops_i64();
  test_ops_u64();
  return 0xCAFEBABE;
}
