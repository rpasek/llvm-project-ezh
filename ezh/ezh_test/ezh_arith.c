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

static __attribute__((noinline, optnone)) void test_ops_i8() {
  TEST_COND(int8_t, 100, 20, +, 120, 0x9001);
  TEST_COND(int8_t, 120, 100, -, 20, 0x9002);
  TEST_COND(int8_t, 12, 10, *, 120, 0x9003);
  TEST_COND(int8_t, 120, 10, /, 12, 0x9004);
  TEST_COND(int8_t, 123, 10, %, 3, 0x9005);
  TEST_COND(int8_t, 1, 3, <<, 8, 0x9006);
  TEST_COND(int8_t, 8, 2, >>, 2, 0x9007);
  TEST_COND(int8_t, 120, 7, +, 127, 0x9008);
  TEST_COND(int8_t, -120, 8, -, -128, 0x9009);
}

static __attribute__((noinline, optnone)) void test_ops_u8() {
  TEST_COND(uint8_t, 200, 50, +, 250, 0x9101);
  TEST_COND(uint8_t, 250, 200, -, 50, 0x9102);
  TEST_COND(uint8_t, 25, 10, *, 250, 0x9103);
  TEST_COND(uint8_t, 250, 10, /, 25, 0x9104);
  TEST_COND(uint8_t, 253, 10, %, 3, 0x9105);
  TEST_COND(uint8_t, 1, 3, <<, 8, 0x9106);
  TEST_COND(uint8_t, 8, 2, >>, 2, 0x9107);
  TEST_COND(uint8_t, 250, 5, +, 255, 0x9108);
  TEST_COND(uint8_t, 250, 10, +, 4, 0x9109);
  TEST_COND(uint8_t, 5, 10, -, 251, 0x910A);
}

static __attribute__((noinline, optnone)) void test_ops_i16() {
  TEST_COND(int16_t, 30000, 2000, +, 32000, 0x9201);
  TEST_COND(int16_t, 32000, 30000, -, 2000, 0x9202);
  TEST_COND(int16_t, 3200, 10, *, 32000, 0x9203);
  TEST_COND(int16_t, 32000, 10, /, 3200, 0x9204);
  TEST_COND(int16_t, 32003, 10, %, 3, 0x9205);
  TEST_COND(int16_t, 1, 3, <<, 8, 0x9206);
  TEST_COND(int16_t, 8, 2, >>, 2, 0x9207);
  TEST_COND(int16_t, 32760, 7, +, 32767, 0x9208);
}

static __attribute__((noinline, optnone)) void test_ops_u16() {
  TEST_COND(uint16_t, 60000, 5000, +, 65000, 0x9301);
  TEST_COND(uint16_t, 65000, 60000, -, 5000, 0x9302);
  TEST_COND(uint16_t, 6500, 10, *, 65000, 0x9303);
  TEST_COND(uint16_t, 65000, 10, /, 6500, 0x9304);
  TEST_COND(uint16_t, 65003, 10, %, 3, 0x9305);
  TEST_COND(uint16_t, 1, 3, <<, 8, 0x9306);
  TEST_COND(uint16_t, 8, 2, >>, 2, 0x9307);
  TEST_COND(uint16_t, 65530, 5, +, 65535, 0x9308);
  TEST_COND(uint16_t, 65530, 10, +, 4, 0x9309);
  TEST_COND(uint16_t, 5, 10, -, 65531, 0x930A);
}

static __attribute__((noinline, optnone)) void test_ops_i32() {
  TEST_COND(int32_t, 2000000000, 100000000, +, 2100000000, 0x9401);
  TEST_COND(int32_t, 2100000000, 2000000000, -, 100000000, 0x9402);
  TEST_COND(int32_t, 210000000, 10, *, 2100000000, 0x9403);
  TEST_COND(int32_t, 2100000000, 10, /, 210000000, 0x9404);
  TEST_COND(int32_t, 2100000003, 10, %, 3, 0x9405);
  TEST_COND(int32_t, 1, 3, <<, 8, 0x9406);
  TEST_COND(int32_t, 8, 2, >>, 2, 0x9407);
  TEST_COND(int32_t, 2147483640, 7, +, 2147483647, 0x9408);
}

static __attribute__((noinline, optnone)) void test_ops_u32() {
  TEST_COND(uint32_t, 4000000000U, 200000000U, +, 4200000000U, 0x9501);
  TEST_COND(uint32_t, 4200000000U, 4000000000U, -, 200000000U, 0x9502);
  TEST_COND(uint32_t, 420000000U, 10U, *, 4200000000U, 0x9503);
  TEST_COND(uint32_t, 4200000000U, 10U, /, 420000000U, 0x9504);
  TEST_COND(uint32_t, 4200000003U, 10U, %, 3U, 0x9505);
  TEST_COND(uint32_t, 1, 3, <<, 8, 0x9506);
  TEST_COND(uint32_t, 8, 2, >>, 2, 0x9507);
  TEST_COND(uint32_t, 4294967290U, 5U, +, 4294967295U, 0x9508);
  TEST_COND(uint32_t, 4294967290U, 10U, +, 4U, 0x9509);
  TEST_COND(uint32_t, 5U, 10U, -, 4294967291U, 0x950A);
}

static __attribute__((noinline, optnone)) void test_ops_i64() {
  TEST_COND(int64_t, 9000000000000000000LL, 200000000000000000LL, +,
            9200000000000000000LL, 0x9601);
  TEST_COND(int64_t, 9200000000000000000LL, 9000000000000000000LL, -,
            200000000000000000LL, 0x9602);
  TEST_COND(int64_t, 920000000000000000LL, 10LL, *, 9200000000000000000LL,
            0x9603);
  TEST_COND(int64_t, 9200000000000000000LL, 10LL, /, 920000000000000000LL,
            0x9604);
  TEST_COND(int64_t, 9200000000000000003LL, 10LL, %, 3LL, 0x9605);
  TEST_COND(int64_t, 1, 3, <<, 8, 0x9606);
  TEST_COND(int64_t, 8, 2, >>, 2, 0x9607);
  TEST_COND(int64_t, 9223372036854775800LL, 7LL, +, 9223372036854775807LL,
            0x9608);

  // New 64-bit shifts custom validation cases!
  TEST_COND(int64_t, 0x802001001LL, 10, >>, 0x2008004LL, 0x960A);
  TEST_COND(int64_t, 0x8000400100000080LL, 38, >>,
            (int64_t)0xFFFFFFFFFE000100ULL, 0x960B);
  TEST_COND(int64_t, 0x4000000058LL, 13, >>, 0x2000000LL, 0x960C);
  TEST_COND(int64_t, -0x4000000058LL, 13, >>, -0x2000001LL, 0x960D);
}

static __attribute__((noinline, optnone)) void test_ops_u64() {
  TEST_COND(uint64_t, 18000000000000000000ULL, 400000000000000000ULL, +,
            18400000000000000000ULL, 0x9701);
  TEST_COND(uint64_t, 18400000000000000000ULL, 18000000000000000000ULL, -,
            400000000000000000ULL, 0x9702);
  TEST_COND(uint64_t, 1840000000000000000ULL, 10ULL, *, 18400000000000000000ULL,
            0x9703);
  TEST_COND(uint64_t, 18400000000000000000ULL, 10ULL, /, 1840000000000000000ULL,
            0x9704);
  TEST_COND(uint64_t, 18400000000000000003ULL, 10ULL, %, 3ULL, 0x9705);
  TEST_COND(uint64_t, 1, 3, <<, 8, 0x9706);
  TEST_COND(uint64_t, 8, 2, >>, 2, 0x9707);
  TEST_COND(uint64_t, 18446744073709551610ULL, 5ULL, +, 18446744073709551615ULL,
            0x9708);
  TEST_COND(uint64_t, 18446744073709551610ULL, 10ULL, +, 4ULL, 0x9709);
  TEST_COND(uint64_t, 5ULL, 10ULL, -, 18446744073709551611ULL, 0x970A);

  // New unsigned 64-bit shifts custom validation cases!
  TEST_COND(uint64_t, 0x802001001ULL, 10, >>, 0x2008004ULL, 0x970B);
  TEST_COND(uint64_t, 0x8000400100000080ULL, 38, >>, 0x2000100ULL, 0x970C);
  TEST_COND(uint64_t, 0x4000000058ULL, 13, >>, 0x2000000ULL, 0x970D);
}

int main() {
  exc_signal = 0x11111111;
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
