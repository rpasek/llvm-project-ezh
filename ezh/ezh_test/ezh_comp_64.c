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

static void test_cond_i64() {
  TEST_COND(int64_t, 10, 20, ==, 0, 0x8601);
  TEST_COND(int64_t, 20, 20, ==, 1, 0x8602);
  TEST_COND(int64_t, 10, 20, !=, 1, 0x8603);
  TEST_COND(int64_t, 20, 20, !=, 0, 0x8604);
  TEST_COND(int64_t, -10, 10, <, 1, 0x8605);
  TEST_COND(int64_t, 10, 10, <, 0, 0x8606);
  TEST_COND(int64_t, -10, 10, <=, 1, 0x8607);
  TEST_COND(int64_t, 10, 10, <=, 1, 0x8608);
  TEST_COND(int64_t, 10, -10, >, 1, 0x8609);
  TEST_COND(int64_t, 10, 10, <, 0, 0x860A);
  TEST_COND(int64_t, 10, -10, >=, 1, 0x860B);
  TEST_COND(int64_t, 10, 10, >=, 1, 0x860C);
  TEST_COND(int64_t, (-9223372036854775807LL - 1LL), 9223372036854775807LL, <,
            1, 0x860D);
  TEST_COND(int64_t, 9223372036854775807LL, (-9223372036854775807LL - 1LL), >,
            1, 0x860E);
}

static __attribute__((noinline, optnone)) void test_cond_u64() {
  TEST_COND(uint64_t, 10, 20, ==, 0, 0x8701);
  TEST_COND(uint64_t, 20, 20, ==, 1, 0x8702);
  TEST_COND(uint64_t, 10, 20, !=, 1, 0x8703);
  TEST_COND(uint64_t, 20, 20, !=, 0, 0x8704);
  TEST_COND(uint64_t, 10, 20, <, 1, 0x8705);
  TEST_COND(uint64_t, 20, 20, <, 0, 0x8706);
  TEST_COND(uint64_t, 10, 20, <=, 1, 0x8707);
  TEST_COND(uint64_t, 20, 20, <=, 1, 0x8708);
  TEST_COND(uint64_t, 20, 10, >, 1, 0x8709);
  TEST_COND(uint64_t, 20, 20, >, 0, 0x870A);
  TEST_COND(uint64_t, 20, 10, >=, 1, 0x870B);
  TEST_COND(uint64_t, 20, 20, >=, 1, 0x870C);
  TEST_COND(uint64_t, 0, 18446744073709551615ULL, <, 1, 0x870D);
  TEST_COND(uint64_t, 18446744073709551615ULL, 0, >, 1, 0x870E);
}

int main() {
  exc_signal = 0x77777777;
  test_cond_i64();
  exc_signal = 0x88888888;
  test_cond_u64();
  return 0xCAFEBABE;
}
