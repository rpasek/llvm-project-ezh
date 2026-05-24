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

static __attribute__((noinline, optnone)) void test_ops_double() {
  TEST_COND(double, 10.5, 5.5, +, 16.0, 0x9901);
  TEST_COND(double, 10.5, 5.5, -, 5.0, 0x9902);
  TEST_COND(double, 10.0, 2.0, *, 20.0, 0x9903);
  TEST_COND(double, 10.0, 2.0, /, 5.0, 0x9904);
  TEST_COND(double, 1.79769313486231e308, 1.79769313486230e308, -,
            9.979201547673599e+293, 0x9905);
}

int main() {
  exc_signal = 0x11111111;
  test_ops_double();
  return 0xCAFEBABE;
}
