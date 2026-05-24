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

static __attribute__((noinline)) void call_longjmp() {
  exc_signal = 0x8001; // Inside helper before longjmp
  longjmp(env, 42);
}

static __attribute__((noinline)) void call_builtin_longjmp() {
  exc_signal = 0x9001; // Inside helper before builtin longjmp
  __builtin_longjmp(builtin_env, 1);
}

int main() {
  exc_signal = 0x11111111;

  // Test 1: Standard setjmp/longjmp
  volatile int val = setjmp(env);
  if (val == 0) {
    exc_signal = 0x8002; // After setjmp direct call
    call_longjmp();
    // Should not be reached!
    while (1)
      ;
  } else if (val == 42) {
    exc_signal = 0xCAFEB001; // Passed standard setjmp/longjmp!
  } else {
    // Wrong val!
    while (1)
      ;
  }

  // Test 2: Compiler __builtin_setjmp / __builtin_longjmp SJLJ
  if (__builtin_setjmp(builtin_env)) {
    exc_signal = 0xCAFEBABE; // Fully passed both SJLJ validation targets!
    return 0xCAFEBABE;
  } else {
    exc_signal = 0x9002; // After builtin_setjmp initialization call
    call_builtin_longjmp();
    // Should not be reached!
    while (1)
      ;
  }

  return 0xBADF00D;
}
