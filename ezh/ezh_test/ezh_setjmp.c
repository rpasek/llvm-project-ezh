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
#include <setjmp.h>

static jmp_buf env;
void *builtin_env[20];

__attribute__((noinline)) void call_longjmp() {
  exc_signal = __LINE__; // Inside helper before longjmp
  longjmp(env, 42);
}

__attribute__((noinline)) void call_builtin_longjmp() {
  exc_signal = __LINE__; // Inside helper before builtin longjmp
  __builtin_longjmp(builtin_env, 1);
}

__attribute__((noinline)) void test_setjmp() {
  // Test 1: Standard setjmp/longjmp
  volatile int val = setjmp(env);
  if (val == 0) {
    exc_signal = __LINE__; // After setjmp direct call
    call_longjmp();
    DEAD();
  } else if (val == 42) {
    exc_signal = __LINE__; // Passed standard setjmp/longjmp!
  } else {
    DEAD();
  }

  // Test 2: Compiler __builtin_setjmp / __builtin_longjmp SJLJ
  if (__builtin_setjmp(builtin_env)) {
    return; // Fully passed both SJLJ validation targets!
  } else {
    exc_signal = __LINE__; // After builtin_setjmp initialization call
    call_builtin_longjmp();
    DEAD();
  }
  DEAD();
}

int main() {
  exc_signal = 0xCAFE0001;
  test_setjmp();
  return 0xCAFEBABE;
}
