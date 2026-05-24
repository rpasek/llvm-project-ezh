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

static __attribute__((noinline, optnone)) void test_cond_float() {
  TEST_COND(float, 1.0f, 2.0f, <, 1, 0x8901);
  TEST_COND(float, 2.0f, 1.0f, >, 1, 0x8902);
  TEST_COND(float, 1.0f, 1.0f, ==, 1, 0x8903);
  TEST_COND(float, 1.0f, 2.0f, !=, 1, 0x8904);
  TEST_COND(float, 1.0f, 2.0f, <=, 1, 0x8905);
  TEST_COND(float, 2.0f, 2.0f, <=, 1, 0x8906);
  TEST_COND(float, 2.0f, 1.0f, >=, 1, 0x8907);
  TEST_COND(float, 2.0f, 2.0f, >=, 1, 0x8908);
  TEST_COND(float, 3.4028234e38f, 3.4028234e38f, ==, 1, 0x8909);
  TEST_COND(float, 1.1754943e-38f, 1.1754943e-38f, ==, 1, 0x890A);
  TEST_COND(float, 3.4028234e38f, 1.0f, >, 1, 0x890B);
  TEST_COND(float, 1.1754943e-38f, 1.0f, <, 1, 0x890C);
}

int main() {
  exc_signal = 0x99999999;
  test_cond_float();
  return 0xCAFEBABE;
}
