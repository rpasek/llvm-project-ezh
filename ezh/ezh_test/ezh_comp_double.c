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

static __attribute__((noinline, optnone)) void test_cond_double() {
  TEST_COND(double, 1.0, 2.0, <, 1, 0x8A01);
  TEST_COND(double, 2.0, 1.0, >, 1, 0x8A02);
  TEST_COND(double, 1.0, 1.0, ==, 1, 0x8A03);
  TEST_COND(double, 1.0, 2.0, !=, 1, 0x8A04);
  TEST_COND(double, 1.0, 2.0, <=, 1, 0x8A05);
  TEST_COND(double, 2.0, 2.0, <=, 1, 0x8A06);
  TEST_COND(double, 2.0, 1.0, >=, 1, 0x8A07);
  TEST_COND(double, 2.0, 2.0, >=, 1, 0x8A08);
  TEST_COND(double, 1.7976931348623157e308, 1.7976931348623157e308, ==, 1,
            0x8A0A);
  TEST_COND(double, 2.2250738585072014e-308, 2.2250738585072014e-308, ==, 1,
            0x8A0B);
  TEST_COND(double, 1.7976931348623157e308, 1.0, >, 1, 0x8A0C);
  TEST_COND(double, 2.2250738585072014e-308, 1.0, <, 1, 0x8A0D);
}

int main() {
  exc_signal = 0xAAAAAAAA;
  test_cond_double();
  return 0xCAFEBABE;
}
