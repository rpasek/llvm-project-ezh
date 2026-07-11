// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ezh_test.h"

volatile uint32_t test_val;

// External function that actually throws!
extern "C" __attribute__((noinline)) void potentially_throwing_func(int x) {
  exc_signal = __LINE__; // Entered potentially_throwing_func
  test_val = x;
  exc_signal = __LINE__; // Before throw
  throw x;             // Throw the integer!
  DEAD();              // After throw (should not be reached)
}

extern "C" __attribute__((noinline)) void test_try_except() {
  exc_signal = __LINE__; // Before try
  try {
    potentially_throwing_func(42);
  } catch (int e) {
    if (e == 42) {
      exc_signal = __LINE__; // Inside catch (successfully caught 42!)
      return;
    }
    DEAD(); // Bad exception value
  }
  DEAD(); // After try-catch (should NOT be reached because we threw)
}

int main() {
  exc_signal = 0xCAFE0001;

  test_try_except();

  return 0xCAFEBABE;
}
