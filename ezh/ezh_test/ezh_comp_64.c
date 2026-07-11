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

void test_cond_i64() {
  TEST_COND(int64_t, 10, 20, ==, 0);
  TEST_COND(int64_t, 20, 20, ==, 1);
  TEST_COND(int64_t, 10, 20, !=, 1);
  TEST_COND(int64_t, 20, 20, !=, 0);
  TEST_COND(int64_t, -10, 10, <, 1);
  TEST_COND(int64_t, 10, 10, <, 0);
  TEST_COND(int64_t, -10, 10, <=, 1);
  TEST_COND(int64_t, 10, 10, <=, 1);
  TEST_COND(int64_t, 10, -10, >, 1);
  TEST_COND(int64_t, 10, 10, <, 0);
  TEST_COND(int64_t, 10, -10, >=, 1);
  TEST_COND(int64_t, 10, 10, >=, 1);
  TEST_COND(int64_t, (-9223372036854775807LL - 1LL), 9223372036854775807LL, <,
            1);
  TEST_COND(int64_t, 9223372036854775807LL, (-9223372036854775807LL - 1LL), >,
            1);
}

__attribute__((noinline, optnone)) void test_cond_u64() {
  TEST_COND(uint64_t, 10, 20, ==, 0);
  TEST_COND(uint64_t, 20, 20, ==, 1);
  TEST_COND(uint64_t, 10, 20, !=, 1);
  TEST_COND(uint64_t, 20, 20, !=, 0);
  TEST_COND(uint64_t, 10, 20, <, 1);
  TEST_COND(uint64_t, 20, 20, <, 0);
  TEST_COND(uint64_t, 10, 20, <=, 1);
  TEST_COND(uint64_t, 20, 20, <=, 1);
  TEST_COND(uint64_t, 20, 10, >, 1);
  TEST_COND(uint64_t, 20, 20, >, 0);
  TEST_COND(uint64_t, 20, 10, >=, 1);
  TEST_COND(uint64_t, 20, 20, >=, 1);
  TEST_COND(uint64_t, 0, 18446744073709551615ULL, <, 1);
  TEST_COND(uint64_t, 18446744073709551615ULL, 0, >, 1);
}

int main() {
  exc_signal = 0xCAFE0001;
  test_cond_i64();
  test_cond_u64();
  return 0xCAFEBABE;
}
