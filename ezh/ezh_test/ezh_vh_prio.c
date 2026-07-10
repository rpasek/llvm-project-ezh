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
 * VECTORED-HOLD PRIORITY MEASUREMENT: when two armed slices are both
 * asserted at the moment acc_vectored_hold samples, which one wins the
 * vector? And does the loser's event survive as "pending" (NVIC-style)?
 *
 * Method (driven by run_vh_prio.sh over the debug probe -- one board):
 * slices 0 and 1 watch software-doorbell trigger channels 0 and 1 in STICKY
 * rising-edge mode. Per round, the core parks in a memory poll (NOT in the
 * hold); the M33 side fires the doorbells -- both flags latch -- then releases
 * the poll; the core enters the hold, which falls straight through and
 * delivers the winner's vector:
 *
 *   round 1: fire ch0 then ch1     (order A)         -> s_v[0]
 *   round 2: fire ch1 then ch0     (order B)         -> s_v[2]
 *   round 3: both in ONE register write (same edge)  -> s_v[4]
 *   round 4: both in one write; after the first dispatch, hold AGAIN with NO
 *            CFM rewrite -> if the loser's sticky flag survived the dispatch,
 *            the second hold falls through with the loser's vector (s_v[7]);
 *            if it blocks forever (s_state stuck at 41), a dispatch consumes
 *            ALL pending flags and there is no queuing.
 *
 * s_state is the JTAG-visible progress marker. Exits 0 -> 0xCAFEBABE only if
 * all four rounds (including the second dispatch) complete.
 */

#include "ezh_test.h"

#define VH_BASE 0x1000u

#define BS0(c) ((c) << 8)
#define BS1(c) ((c) << 11)
#define BS2(c) ((c) << 14)
#define BS3(c) ((c) << 17)
#define BS4(c) ((c) << 20)
#define BS5(c) ((c) << 23)
#define BS6(c) ((c) << 26)
#define BS7(c) ((c) << 29)
#define BS_RISE 1u
#define BS_0 6u
#define REST_FORCE0                                                            \
  (BS2(BS_0) | BS3(BS_0) | BS4(BS_0) | BS5(BS_0) | BS6(BS_0) | BS7(BS_0))

volatile unsigned s_ready = 0, s_go = 0, s_state = 0;
volatile unsigned s_v[8];

static unsigned slice_of(void *vec) {
  return (((unsigned)vec - VH_BASE) >> 2) - 1u;
}

int main(void) {
  unsigned cfm = BS0(BS_RISE) | BS1(BS_RISE) | REST_FORCE0 | 0xFFu;
  __builtin_ezh_write_cfs(BS0(0u) | BS1(1u));
  __builtin_ezh_write_cfm(cfm);
  __builtin_ezh_write_cfm(__builtin_ezh_read_cfm()); /* clear stale flags */

  for (unsigned r = 1u; r <= 4u; r++) {
    s_ready = r;      /* tell the driver we are parked outside the hold */
    s_state = r * 10u;
    while (s_go != r) { /* driver fires the doorbells, then releases us */
    }
    void *v = __builtin_ezh_acc_vectored_hold((void *)VH_BASE, 0x3u);
    s_v[(r - 1u) * 2u] = slice_of(v);
    s_state = r * 10u + 1u;

    if (r == 4u) { /* queue test: NO CFM rewrite between the two holds */
      void *v2 = __builtin_ezh_acc_vectored_hold((void *)VH_BASE, 0x3u);
      s_v[7] = slice_of(v2);
      s_state = 42u;
    }

    __builtin_ezh_write_cfm(cfm); /* clean slate for the next round */
  }
  return 0; /* crt0 -> exc_signal = 0xCAFEBABE */
}
