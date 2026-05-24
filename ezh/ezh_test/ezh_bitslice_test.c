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

#include "ezh.h"
#include "ezh_test.h"

static volatile int interrupt_triggered = 0;
extern volatile uint32_t bitslice_cfm_backup;

#define VECTOR 6
void __attribute__((used)) vector6() { interrupt_triggered = 1; }

static volatile int captured_ZE;
static volatile int captured_PO;
static volatile int captured_CA;
#define RUN_ALU_TEST(state_set_asm, expected_ZE, expected_PO, expected_CA,     \
                     test_id)                                                  \
  do {                                                                         \
    interrupt_triggered = 0;                                                   \
    captured_ZE = -1;                                                          \
    captured_PO = -1;                                                          \
    captured_CA = -1;                                                          \
    /* Trigger interrupt */                                                    \
    EZH->PENDTRAP = 0;                                                         \
    EZH->PENDTRAP = (1 << (16 + VECTOR)) | (1 << VECTOR);                      \
    __asm__ volatile(state_set_asm                                             \
                     "\n\t"                                                    \
                     "1:\n\t"                                                  \
                     "goto_nbs 1b\n\t"                                         \
                     "gotol_bs bitslice_handler\n\t" /* Capture flags */       \
                     "load_imm %0, 0\n\t"                                      \
                     "bset_imm_ze %0, %0, 0\n\t"                               \
                     "load_imm %1, 0\n\t"                                      \
                     "bset_imm_po %1, %1, 0\n\t"                               \
                     "load_imm %2, 0\n\t"                                      \
                     "bset_imm_ca %2, %2, 0\n\t"                               \
                     : "=r"(captured_ZE), "=r"(captured_PO), "=r"(captured_CA) \
                     :                                                         \
                     : "r1");                                                  \
    if (interrupt_triggered != 1) {                                            \
      exc_signal = 0xBADF0000 | test_id;                                       \
      return exc_signal;                                                       \
    }                                                                          \
    if (captured_ZE != expected_ZE || captured_PO != expected_PO ||            \
        captured_CA != expected_CA) {                                          \
      exc_signal = 0xBAD00000 | (test_id << 12) | (captured_ZE << 8) |         \
                   (captured_PO << 4) | captured_CA;                           \
      return exc_signal;                                                       \
    }                                                                          \
  } while (0)

static void prove_edges_conclusively() {
  volatile int bs_status = -1;
  volatile int cfs_status = -1;
  volatile uint32_t dummy;

  // 1. Start with clean state
  EZH->PENDTRAP = 0;
  dummy = EZH->PENDTRAP;
  (void)dummy;
  ezh_write_cfm(ezh_read_cfm()); // Clear BS flag

  // Read bs status (should be 0)
  __asm__ volatile("load_imm %0, 0\n\t"
                   "bset_imm_bs %0, %0, 0\n\t"
                   : "=r"(bs_status));
  cfs_status = ezh_read_cfs() & (1 << VECTOR);

  if (bs_status != 0 || cfs_status != 0) {
    exc_signal = 0xBAD01000 | (bs_status << 8) | (cfs_status ? 1 : 0);
    while (1)
      ;
  }

  // 2. Trigger (0 -> 1 transition)
  EZH->PENDTRAP = (1 << (16 + VECTOR)) | (1 << VECTOR);
  dummy = EZH->PENDTRAP;
  (void)dummy;

  // Wait a bit to ensure hardware propagates (synchronous, should be instant)
  __asm__ volatile("load_imm %0, 0\n\t"
                   "bset_imm_bs %0, %0, 0\n\t"
                   : "=r"(bs_status));
  cfs_status = ezh_read_cfs() & (1 << VECTOR);

  if (bs_status != 1 || cfs_status == 0) {
    exc_signal = 0xBAD02000 | (bs_status << 8) | (cfs_status ? 1 : 0);
    while (1)
      ;
  }

  // 3. Clear BS flag while input is still 1
  ezh_write_cfm(ezh_read_cfm()); // Clear BS flag

  // Read bs status again.
  // If BS_RISE (edge) is working: bs_status should be 0 (because no new edge
  // occurred). If BS_SIG (level) was active: bs_status would be 1 (because
  // level is still 1).
  __asm__ volatile("load_imm %0, 0\n\t"
                   "bset_imm_bs %0, %0, 0\n\t"
                   : "=r"(bs_status));
  cfs_status = ezh_read_cfs() & (1 << VECTOR);

  if (bs_status != 0) {
    // FAIL: bs is still active even after clear! This indicates level-sensitive
    // behavior!
    exc_signal = 0xBAD03000 | (bs_status << 8) | (cfs_status ? 1 : 0);
    while (1)
      ;
  }
  if (cfs_status == 0) {
    // Sanity check: input must still be 1!
    exc_signal = 0xBAD04000 | (bs_status << 8) | (cfs_status ? 1 : 0);
    while (1)
      ;
  }

  // 4. Now do a new transition: 1 -> 0 -> 1
  EZH->PENDTRAP = 0; // input to 0
  dummy = EZH->PENDTRAP;
  (void)dummy;
  ezh_write_cfm(ezh_read_cfm()); // Clear BS flag

  // Verify bs is 0, cfs is 0
  __asm__ volatile("load_imm %0, 0\n\t"
                   "bset_imm_bs %0, %0, 0\n\t"
                   : "=r"(bs_status));
  cfs_status = ezh_read_cfs() & (1 << VECTOR);
  if (bs_status != 0 || cfs_status != 0) {
    exc_signal = 0xBAD05000 | (bs_status << 8) | (cfs_status ? 1 : 0);
    while (1)
      ;
  }

  // Trigger again (0 -> 1 transition)
  EZH->PENDTRAP = (1 << (16 + VECTOR)) | (1 << VECTOR);
  dummy = EZH->PENDTRAP;
  (void)dummy;

  // Verify it triggers again!
  __asm__ volatile("load_imm %0, 0\n\t"
                   "bset_imm_bs %0, %0, 0\n\t"
                   : "=r"(bs_status));
  cfs_status = ezh_read_cfs() & (1 << VECTOR);

  if (bs_status != 1 || cfs_status == 0) {
    exc_signal = 0xBAD06000 | (bs_status << 8) | (cfs_status ? 1 : 0);
    while (1)
      ;
  }

  // Clean up
  EZH->PENDTRAP = 0;
}

int main() {
  exc_signal = 0x11111111;

  // Enable bit slice 7 to trigger on a high signal (and enable all slices)
  ezh_write_cfm(BS7(BS_0) | BS6(BS_RISE) | BS5(BS_0) | BS4(BS_0) | BS3(BS_0) |
                BS2(BS_0) | BS1(BS_0) | BS0(BS_0) | 0xFF);
  /* Configure all bit slices to take inputs from a unique channel (doesn't
     matter for this test) */
  ezh_write_cfs(BS7(EZH_INPUT_SOURCE_7) | BS6(EZH_INPUT_SOURCE_6) |
                BS5(EZH_INPUT_SOURCE_5) | BS4(EZH_INPUT_SOURCE_4) |
                BS3(EZH_INPUT_SOURCE_3) | BS2(EZH_INPUT_SOURCE_2) |
                BS1(EZH_INPUT_SOURCE_1) | BS0(EZH_INPUT_SOURCE_0) | 0x0);

  // State 0: ZE=0, PO=0, CA=0
  RUN_ALU_TEST("load_imms r1, -1", 0, 0, 0, 0);

  // State 2: ZE=0, PO=1, CA=0
  RUN_ALU_TEST("load_imms r1, 1", 0, 1, 0, 2);

  // State 3: ZE=1, PO=1, CA=0
  RUN_ALU_TEST("load_imms r1, 0", 1, 1, 0, 3);

  // State 4: ZE=0, PO=0, CA=1
  RUN_ALU_TEST("load_imm r1, 0\n\tsub_imms r1, r1, 1", 0, 0, 1, 4);

  // State 6: ZE=0, PO=1, CA=1
  RUN_ALU_TEST("load_imm r1, 0\n\tsub_imms r1, r1, -1", 0, 1, 1, 6);

  // State 7: ZE=1, PO=1, CA=1
  RUN_ALU_TEST("load_imm r1, 0\n\tsub_imm r1, r1, 1\n\tadd_imms r1, r1, 1", 1,
               1, 1, 7);

  prove_edges_conclusively();

  exc_signal = (int)0xCAFEBABE;
  return (int)0xCAFEBABE;
}
