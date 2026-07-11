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

__attribute__((noinline, optnone)) void test_cond_double() {
  TEST_COND(double, 1.0, 2.0, <, 1);
  TEST_COND(double, 2.0, 1.0, >, 1);
  TEST_COND(double, 1.0, 1.0, ==, 1);
  TEST_COND(double, 1.0, 2.0, !=, 1);
  TEST_COND(double, 1.0, 2.0, <=, 1);
  TEST_COND(double, 2.0, 2.0, <=, 1);
  TEST_COND(double, 2.0, 1.0, >=, 1);
  TEST_COND(double, 2.0, 2.0, >=, 1);
  TEST_COND(double, 1.7976931348623157e308, 1.7976931348623157e308, ==, 1);
  TEST_COND(double, 2.2250738585072014e-308, 2.2250738585072014e-308, ==, 1);
  TEST_COND(double, 1.7976931348623157e308, 1.0, >, 1);
  TEST_COND(double, 2.2250738585072014e-308, 1.0, <, 1);
}

int main() {
  exc_signal = 0xCAFE0001;
  test_cond_double();
  return 0xCAFEBABE;
}
