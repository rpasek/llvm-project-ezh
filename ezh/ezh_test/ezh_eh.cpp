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

// External function that actually throws!
extern "C" __attribute__((noinline)) void potentially_throwing_func(int x) {
  exc_signal = 0xEE10; // Entered potentially_throwing_func
  test_val = x;
  exc_signal = 0xEE11; // Before throw
  throw x;             // Throw the integer!
  exc_signal = 0xEE12; // After throw (should not be reached)
}

extern "C" __attribute__((noinline)) uint32_t test_try_except() {
  exc_signal = 0xEE01; // Before try
  try {
    potentially_throwing_func(42);
  } catch (int e) {
    if (e == 42) {
      exc_signal = 0xEE02; // Inside catch (successfully caught 42!)
      return 0xCAFEBABE;
    }
    exc_signal = 0xEE99; // Bad exception value
    return 0xBAD;
  }
  exc_signal =
      0xEE03; // After try-catch (should NOT be reached because we threw)
  return 0xBAD;
}

int main() {
  exc_signal = 0x11111111;

  uint32_t res = test_try_except();
  if (res != 0xCAFEBABE) {
    while (1)
      ;
  }

  exc_signal = 0xCAFEBABE;
  return 0xCAFEBABE;
}
