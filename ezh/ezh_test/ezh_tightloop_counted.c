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

/*
 * Decisive experiment: does tight_loop iterate a PLAIN counted body with no
 * event pacing, once the AN14650 run-once-slot restriction is respected?
 *
 * AN14650 E_TIGHT_LOOP semantics under test:
 *  - "The instruction immediately following E_TIGHT_LOOP is executed only
 *     once. The following instruction is considered the start of the loop."
 *  - Rend = address of the instruction after the last repeated opcode.
 *  - Rcount = repeats AFTER the initial execution (block runs Rcount+1 times).
 *
 * The 2026-06 free-running experiments predate this restriction and used
 * 1-instruction bodies -- entirely swallowed by the run-once slot, which
 * explains "ran zero times". Every case below places an explicit nop in the
 * slot and measures executions with register counters read back afterwards.
 * All counter inits use volatile asm so the compiler cannot sink them into
 * the loop region (it did, when they were plain C assignments).
 */

#include "ezh_test.h"

volatile unsigned results[10];
static volatile unsigned buf[12];

#define INIT0(v) __asm volatile("load_imm %0, 0" : "=r"(v))

int main() {
  /* Case A: nop slot + 1-insn repeated block, count=4 -> body runs 5x. */
  {
    unsigned iter;
    INIT0(iter);
    void *rend = &&done_a;
    __builtin_ezh_tight_loop(rend, 4);
    __asm volatile("nop");                            /* run-once slot */
    __asm volatile("add_imm %0, %0, 1" : "+r"(iter)); /* repeated block */
  done_a:
    results[0] = iter;
    if (iter != 5)
      return 0xE1A00000 | iter;
  }

  /* Case B: count=0 -> the block still runs its initial execution once. */
  {
    unsigned iter;
    INIT0(iter);
    void *rend = &&done_b;
    __builtin_ezh_tight_loop(rend, 0);
    __asm volatile("nop");
    __asm volatile("add_imm %0, %0, 1" : "+r"(iter));
  done_b:
    results[1] = iter;
    if (iter != 1)
      return 0xE1B00000 | iter;
  }

  /* Case C: the slot instruction itself must run exactly ONCE while the
     block repeats: slot-add + repeated-add, count=4 -> slot=1, body=5. */
  {
    unsigned slot, iter;
    INIT0(slot);
    INIT0(iter);
    void *rend = &&done_c;
    __builtin_ezh_tight_loop(rend, 4);
    __asm volatile("add_imm %0, %0, 1" : "+r"(slot)); /* run-once slot */
    __asm volatile("add_imm %0, %0, 1" : "+r"(iter)); /* repeated block */
  done_c:
    results[2] = slot;
    results[3] = iter;
    if (slot != 1)
      return 0xE0C00000 | slot;
    if (iter != 5)
      return 0xE1C00000 | iter;
  }

  /* Case D: multi-instruction block ordering, count=9 -> a=10, b=1+..+10. */
  {
    unsigned a, b;
    INIT0(a);
    INIT0(b);
    void *rend = &&done_d;
    __builtin_ezh_tight_loop(rend, 9);
    __asm volatile("nop");
    __asm volatile("add_imm %0, %0, 1" : "+r"(a));
    __asm volatile("add %0, %0, %1" : "+r"(b) : "r"(a)); /* b += a */
  done_d:
    results[4] = a;
    results[5] = b;
    if (a != 10)
      return 0xE1D00000 | a;
    if (b != 55)
      return 0xE2D00000 | b;
  }

  /* Case E: the DMA-pump shape -- post-incrementing store body, count=7 ->
     8 stores of 1..8. */
  {
    unsigned val;
    volatile unsigned *p = buf;
    INIT0(val);
    /* Force p's materialization BEFORE the loop region (it sank into the
       repeated block as a plain C init, resetting the pointer every iteration). */
    __asm volatile("" : "+r"(p));
    void *rend = &&done_e;
    __builtin_ezh_tight_loop(rend, 7);
    __asm volatile("nop");
    __asm volatile("add_imm %0, %0, 1\n\tstr_post %1, %0, 4"
                   : "+r"(val), "+r"(p)); /* val++; *p++ = val */
  done_e:
    results[6] = val;
    results[7] = (unsigned)(p - buf);
    if (val != 8)
      return 0xE1E00000 | val;
    if (buf[0] != 1 || buf[7] != 8 || buf[8] != 0)
      return 0xE2E00000 | buf[7];
  }

  return 0xCAFEBABE;
}
