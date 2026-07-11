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

extern void __ezh_bitslice_handler();

// =============================================================================
// PHASE 3: Subroutine Flow Control stepping targets
// =============================================================================

// [Phase 3 - Step 3: step over test_3_subroutine_step_target() call line
// directly into subroutine!]
__attribute__((noinline, optnone)) void test_3_subroutine_step_target() {
  exc_signal = 0x99999999; // Second breakpoint target signature!
}

// [Phase 3 - Step 1: step over function-entry synchronization NOPs to
// exc_signal line]
__attribute__((noinline, optnone)) void test_3_subroutine_step() {
  __asm__ volatile("nop\n\t"
                   "nop\n\t");
  // [Phase 3 - Step 2: step over exc_signal line to
  // test_3_subroutine_step_target() call line]
  exc_signal = 0x88888888; // First subroutine entry signature
  test_3_subroutine_step_target();
}

// =============================================================================
// PHASE 1: Connecting, Resets, Ignition, and Register Tracking
// =============================================================================

// [Test 1B: Target Ignition via Breakpoint Continue]
// [Test 1C: Single-Step PC-Relative Load Corruption Proof]
__attribute__((noinline, optnone)) void
test_1b_1c_ignition_and_corruption() {

  __asm__ volatile(
      // 1. Set registers r0-r7, gpo, gpd
      "load_imm r0, 0x11\n\t" /* [Test 1C - Step 4: e_load_imm r0] */
      "load_imm r1, 0x22\n\t" /* [Test 1C - Step 5: e_load_imm r1] */
      "load_imm r2, 0x33\n\t" /* [Test 1C - Step 6: e_load_imm r2] */
      "load_imm r3, 0x44\n\t" /* [Test 1C - Step 7: e_load_imm r3] */
      "load_imm r4, 0x55\n\t" /* [Test 1C - Step 8: e_load_imm r4] */
      "load_imm r5, 0x66\n\t" /* [Test 1C - Step 9: e_load_imm r5] */
      "load_imm r6, 0x77\n\t" /* [Test 1C - Step 10: e_load_imm r6] */
      /* Note: We do NOT clobber R7 here. R7 is used as the Frame Pointer in
         this function (forced by 'optnone'). Clobbering the Frame Pointer is
         unsafe as it is used to restore the Stack Pointer (SP) on return.*/
      "load_imm gpo, 0x99\n\t" /* [Test 1C - Step 11: e_load_imm gpo] */
      "load_imm gpd, 0xaa\n\t" /* [Test 1C - Step 12: e_load_imm gpd] */

      // 2. Clear registers r0-r6, gpo, gpd
      "load_imm r0, 0\n\t"
      "load_imm r1, 0\n\t"
      "load_imm r2, 0\n\t"
      "load_imm r3, 0\n\t"
      "load_imm r4, 0\n\t"
      "load_imm r5, 0\n\t"
      "load_imm r6, 0\n\t"
      "load_imm gpo, 0\n\t"
      "load_imm gpd, 0\n\t"

      // 3. Math to set ALU flags
      // PO, AZ, NZ (Positive, Above Zero, Not Zero)
      "load_imms r0, 1\n\t"

      // ZE, ZB, CZ (Zero, Zero or Below, Carry or Zero)
      "load_imms r0, 0\n\t"

      // NE, NZ, ZB (Negative, Not Zero, Zero or Below)
      "load_imms r0, -1\n\t"

      // CA, ZE, ZB, CZ (Carry, Zero, Zero or Below, Carry or Zero) via unsigned
      // overflow
      "add_imms r0, r0, 1\n\t"
      :
      :
      : "r0", "r1", "r2", "r3", "r4", "r5", "r6");
}

// =============================================================================
// PHASE 2: Next Instruction Prediction for ALU Condition Codes
// =============================================================================

// [Continuous-only verified (JTAG single-step hardware register-patching
// verified)]
__attribute__((noinline)) void test_cond_branches() {
  exc_signal = 0x66666666;
  __asm__ volatile(
      // 1. Test ZE (Zero)
      "load_imms r0, 0\n\t" // Sets ZE=1
      "goto_ze branch_ze_true\n\t"
      "nop\n\t"
      "branch_ze_true:\n\t"
      "load_imms r0, 1\n\t" // Sets ZE=0
      "goto_ze branch_ze_false\n\t"
      "nop\n\t"
      "branch_ze_false:\n\t"

      // 2. Test NZ (Not Zero)
      "load_imms r0, 1\n\t" // Sets NZ=1
      "goto_nz branch_nz_true\n\t"
      "nop\n\t"
      "branch_nz_true:\n\t"
      "load_imms r0, 0\n\t" // Sets NZ=0
      "goto_nz branch_nz_false\n\t"
      "nop\n\t"
      "branch_nz_false:\n\t"

      // 3. Test PO (Positive)
      "load_imms r0, 1\n\t" // Sets PO=1
      "goto_po branch_po_true\n\t"
      "nop\n\t"
      "branch_po_true:\n\t"
      "load_imms r0, -1\n\t" // Sets PO=0 (Negative)
      "goto_po branch_po_false\n\t"
      "nop\n\t"
      "branch_po_false:\n\t"

      // 4. Test NE (Negative)
      "load_imms r0, -1\n\t" // Sets NE=1
      "goto_ne branch_ne_true\n\t"
      "nop\n\t"
      "branch_ne_true:\n\t"
      "load_imms r0, 1\n\t" // Sets NE=0
      "goto_ne branch_ne_false\n\t"
      "nop\n\t"
      "branch_ne_false:\n\t"

      // 5. Test AZ (Above Zero)
      "load_imms r0, 1\n\t" // Sets AZ=1
      "goto_az branch_az_true\n\t"
      "nop\n\t"
      "branch_az_true:\n\t"
      "load_imms r0, 0\n\t" // Sets AZ=0
      "goto_az branch_az_false\n\t"
      "nop\n\t"
      "branch_az_false:\n\t"

      // 6. Test ZB (Zero or Below)
      "load_imms r0, 0\n\t" // Sets ZB=1
      "goto_zb branch_zb_true\n\t"
      "nop\n\t"
      "branch_zb_true:\n\t"
      "load_imms r0, 1\n\t" // Sets ZB=0
      "goto_zb branch_zb_false\n\t"
      "nop\n\t"
      "branch_zb_false:\n\t"

      // 7. Test CA (Carry)
      "load_imms r0, -1\n\t"
      "add_imms r0, r0, 1\n\t" // Sets CA=1 via unsigned overflow
      "goto_ca branch_ca_true\n\t"
      "nop\n\t"
      "branch_ca_true:\n\t"
      "load_imms r0, 0\n\t" // Sets CA=0
      "goto_ca branch_ca_false\n\t"
      "nop\n\t"
      "branch_ca_false:\n\t"

      // 8. Test NC (No Carry)
      "load_imms r0, 0\n\t" // Sets NC=1
      "goto_nc branch_nc_true\n\t"
      "nop\n\t"
      "branch_nc_true:\n\t"
      "load_imms r0, -1\n\t"
      "add_imms r0, r0, 1\n\t" // Sets CA=1, NC=0
      "goto_nc branch_nc_false\n\t"
      "nop\n\t"
      "branch_nc_false:\n\t"

      // 9. Test CZ (Carry or Zero)
      "load_imms r0, 0\n\t" // Sets CZ=1
      "goto_cz branch_cz_true\n\t"
      "nop\n\t"
      "branch_cz_true:\n\t"
      "load_imms r0, 1\n\t" // Sets CZ=0
      "goto_cz branch_cz_false\n\t"
      "nop\n\t"
      "branch_cz_false:\n\t"
      :
      :
      : "r0");
}

// =============================================================================
// PHASE 3B: Consecutive Stepping inside C Conditional Branches
// =============================================================================

__attribute__((noinline, optnone)) void test_4_c_stepping() {
  __asm__ volatile("nop\n\t"
                   "nop\n\t");
  volatile int x = 1;

  if (x == 1) {
    exc_signal = 0xaaaa0001; // Taken!
  } else {
    exc_signal = 0xaaaa0002; // Not Taken!
  }

  if (x == 2) {
    exc_signal = 0xaaaa0003; // Not Taken!
  } else {
    exc_signal = 0xaaaa0004; // Taken!
  }
}

// =============================================================================
// PHASE 7: Multiple Breakpoints Verification
// =============================================================================
__attribute__((noinline, optnone)) void test_7_multi_breakpoints() {
  volatile int a = 0;
  volatile int b = 0;
  volatile int c = 0;

  a = 10; // BP 1
  b = 20; // BP 2
  c = 30; // BP 3

  (void)a;
  (void)b;
  (void)c;
}

// =============================================================================
// PHASE 8: Breakpoint Stepping and Resuming Verification
// =============================================================================
__attribute__((noinline, optnone)) void
test_8_breakpoint_stepping_and_resuming() {
  __asm__ volatile("nop\n\t"
                   "nop\n\t");
  exc_signal = 0x77770001; // Step over target!
  exc_signal = 0x77770002; // Continue over target!
  exc_signal = 0x77770003; // Final target!
}

__attribute__((noinline)) int test_abi_add(int a, int b, int c, int d) {
  return a + b + c + d;
}

int main() {
  volatile int counter = 0;
  while (1) {
    counter++;
    exc_signal = 0x11112222; // Loop running signature
    test_1b_1c_ignition_and_corruption();
    test_cond_branches();
    test_3_subroutine_step();
    test_4_c_stepping();
    test_7_multi_breakpoints();
    test_8_breakpoint_stepping_and_resuming();
    volatile int res = test_abi_add(10, 20, 30, 40);
    (void)res;
  }
  return 0xCAFEBABE;
}
