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

static __attribute__((noinline, optnone)) void test_cond_i8() {
  TEST_COND(int8_t, 10, 20, ==, 0, 0x8001);
  TEST_COND(int8_t, 20, 20, ==, 1, 0x8002);
  TEST_COND(int8_t, 10, 20, !=, 1, 0x8003);
  TEST_COND(int8_t, 20, 20, !=, 0, 0x8004);
  TEST_COND(int8_t, -10, 10, <, 1, 0x8005);
  TEST_COND(int8_t, 10, 10, <, 0, 0x8006);
  TEST_COND(int8_t, -10, 10, <=, 1, 0x8007);
  TEST_COND(int8_t, 10, 10, <=, 1, 0x8008);
  TEST_COND(int8_t, 10, -10, >, 1, 0x8009);
  TEST_COND(int8_t, 10, 10, >, 0, 0x800A);
  TEST_COND(int8_t, 10, -10, >=, 1, 0x800B);
  TEST_COND(int8_t, 10, 10, >=, 1, 0x800C);
  TEST_COND(int8_t, -128, 127, <, 1, 0x800D);
  TEST_COND(int8_t, 127, -128, >, 1, 0x800E);
}

static __attribute__((noinline, optnone)) void test_cond_u8() {
  TEST_COND(uint8_t, 10, 20, ==, 0, 0x8101);
  TEST_COND(uint8_t, 20, 20, ==, 1, 0x8102);
  TEST_COND(uint8_t, 10, 20, !=, 1, 0x8103);
  TEST_COND(uint8_t, 20, 20, !=, 0, 0x8104);
  TEST_COND(uint8_t, 10, 20, <, 1, 0x8105);
  TEST_COND(uint8_t, 20, 20, <, 0, 0x8106);
  TEST_COND(uint8_t, 10, 20, <=, 1, 0x8107);
  TEST_COND(uint8_t, 20, 20, <=, 1, 0x8108);
  // TEST_COND(uint8_t, 20, 10, >, 1, 0x8109);
  TEST_COND(uint8_t, 20, 20, >, 0, 0x810A);
  TEST_COND(uint8_t, 20, 10, >=, 1, 0x810B);
  TEST_COND(uint8_t, 20, 20, >=, 1, 0x810C);
  TEST_COND(uint8_t, 0, 255, <, 1, 0x810D);
  TEST_COND(uint8_t, 255, 0, >, 1, 0x810E);
}

static __attribute__((noinline, optnone)) void test_cond_i16() {
  TEST_COND(int16_t, 10, 20, ==, 0, 0x8201);
  TEST_COND(int16_t, 20, 20, ==, 1, 0x8202);
  TEST_COND(int16_t, 10, 20, !=, 1, 0x8203);
  TEST_COND(int16_t, 20, 20, !=, 0, 0x8204);
  TEST_COND(int16_t, -10, 10, <, 1, 0x8205);
  TEST_COND(int16_t, 10, 10, <, 0, 0x8206);
  TEST_COND(int16_t, -10, 10, <=, 1, 0x8207);
  TEST_COND(int16_t, 10, 10, <=, 1, 0x8208);
  TEST_COND(int16_t, 10, -10, >, 1, 0x8209);
  TEST_COND(int16_t, 10, 10, >, 0, 0x820A);
  TEST_COND(int16_t, 10, -10, >=, 1, 0x820B);
  TEST_COND(int16_t, 10, 10, >=, 1, 0x820C);
  TEST_COND(int16_t, -32768, 32767, <, 1, 0x820D);
  TEST_COND(int16_t, 32767, -32768, >, 1, 0x820E);
}

static __attribute__((noinline, optnone)) void test_cond_u16() {
  TEST_COND(uint16_t, 10, 20, ==, 0, 0x8301);
  TEST_COND(uint16_t, 20, 20, ==, 1, 0x8302);
  TEST_COND(uint16_t, 10, 20, !=, 1, 0x8303);
  TEST_COND(uint16_t, 20, 20, !=, 0, 0x8304);
  TEST_COND(uint16_t, 10, 20, <, 1, 0x8305); // this one
  TEST_COND(uint16_t, 20, 20, <, 0, 0x8306);
  TEST_COND(uint16_t, 10, 20, <=, 1, 0x8307);
  TEST_COND(uint16_t, 20, 20, <=, 1, 0x8308);
  TEST_COND(uint16_t, 20, 10, >, 1, 0x8309);
  TEST_COND(uint16_t, 20, 20, >, 0, 0x830A);
  TEST_COND(uint16_t, 20, 10, >=, 1, 0x830B);
  TEST_COND(uint16_t, 20, 20, >=, 1, 0x830C);
  TEST_COND(uint16_t, 0, 65535, <, 1, 0x830D);
  TEST_COND(uint16_t, 65535, 0, >, 1, 0x830E);
}

static __attribute__((noinline, optnone)) void test_cond_i32() {
  TEST_COND(int32_t, 10, 20, ==, 0, 0x8401);
  TEST_COND(int32_t, 20, 20, ==, 1, 0x8402);
  TEST_COND(int32_t, 10, 20, !=, 1, 0x8403);
  TEST_COND(int32_t, 20, 20, !=, 0, 0x8404);
  TEST_COND(int32_t, -10, 10, <, 1, 0xBBBB);
  TEST_COND(int32_t, 10, 10, <, 0, 0x8406);
  TEST_COND(int32_t, -10, 10, <=, 1, 0x8407);
  TEST_COND(int32_t, 10, 10, <=, 1, 0x8408);
  TEST_COND(int32_t, 10, -10, >, 1, 0x8409);
  TEST_COND(int32_t, 10, 10, >, 0, 0x840A);
  TEST_COND(int32_t, 10, -10, >=, 1, 0x840B);
  TEST_COND(int32_t, 10, 10, >=, 1, 0x840C);
  TEST_COND(int32_t, -2147483648LL, 2147483647LL, <, 1, 0x840D);
  TEST_COND(int32_t, 2147483647LL, -2147483648LL, >, 1, 0x840E);
}

static __attribute__((noinline, optnone)) void test_cond_u32() {
  TEST_COND(uint32_t, 10, 20, ==, 0, 0x8501);
  TEST_COND(uint32_t, 20, 20, ==, 1, 0x8502);
  TEST_COND(uint32_t, 10, 20, !=, 1, 0x8503);
  TEST_COND(uint32_t, 20, 20, !=, 0, 0x8504);
  TEST_COND(uint32_t, 10, 20, <, 1, 0x8505);
  TEST_COND(uint32_t, 20, 20, <, 0, 0x8506);
  TEST_COND(uint32_t, 10, 20, <=, 1, 0x8507);
  TEST_COND(uint32_t, 20, 20, <=, 1, 0x8508);
  TEST_COND(uint32_t, 20, 10, >, 1, 0x8509);
  TEST_COND(uint32_t, 20, 20, >, 0, 0x850A);
  TEST_COND(uint32_t, 20, 10, >=, 1, 0x850B);
  TEST_COND(uint32_t, 20, 20, >=, 1, 0x850C);
  TEST_COND(uint32_t, 0, 4294967295U, <, 1, 0x850D);
  TEST_COND(uint32_t, 4294967295U, 0, >, 1, 0x850E);
}

int main() {
  exc_signal = 0x11111111;
  test_cond_i8();
  exc_signal = 0x22222222;
  test_cond_u8();
  exc_signal = 0x33333333;
  test_cond_i16();
  exc_signal = 0x44444444;
  test_cond_u16();
  exc_signal = 0x55555555;
  test_cond_i32();
  exc_signal = 0x66666666;
  test_cond_u32();
  return 0xCAFEBABE;
}
