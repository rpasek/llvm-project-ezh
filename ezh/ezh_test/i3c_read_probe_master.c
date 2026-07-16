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
 * TRUE I3C (SDR mode) MASTER, fully interrupt-driven -- no polling.
 *
 * Unlike the earlier demos (which drove the I3C0 block in I2C-legacy mode,
 * MCTRL.TYPE=1), every message here is native I3C SDR (TYPE=0): open-drain
 * headers at ~100 kHz, push-pull data phases at ~1 MHz, and a real
 * dynamic-address assignment before any data moves:
 *
 *   1. RSTDAA broadcast (clears any stale dynamic address), then the real
 *      ENTDAA ceremony (MCTRL.REQUEST = ProcessDAA): the master collects the
 *      slave's 8 DAA ID bytes (48-bit PID + BCR + DCR, captured in
 *      m_pid_lo/hi + m_bcrdcr) and assigns dynamic address 0x30.
 *   2. Private SDR write to 0x30: the 17 payload bytes [0x00, 0x10..0x1F].
 *
 * Every phase is interrupt-gated: the firmware points MINTSET at the events
 * it needs next (MCTRLDONE/RXPEND/COMPLETE for the DAA, TXNOTFULL for data
 * pacing) and sleeps in __builtin_ezh_hold() until the I3C0 IRQ (routed
 * INPUTMUX ch0 -> bitslice 0) wakes it. Status bits are W1C and the IRQ is a
 * level, so the waits are race-free: an event that fires early leaves the
 * line high and the hold falls straight through.
 *
 * HARD-WON BRING-UP FACTS (see run_i3c_sdr.sh for the M33-side config):
 *  - The I3C block's CCC/DAA engine and repeated-START sequencing run off the
 *    separate TIME-CONTROL (slow) clock -- LPOSC 1 MHz via CLKCTL1
 *    I3C0FCLKSTCSEL/-STCDIV/-SDIV. Without it, Sr requests hang forever and
 *    CCCs are silently ignored.
 *  - The pins need FULL DRIVE + slew, no internal pulls (IOPCTL 0x1C1, PUR
 *    0x181). With the weak-drive 0x71 config, private SDR writes work but the
 *    PUR-dependent open-drain turnarounds of every CCC/DAA are corrupted --
 *    the slave ACKs 0x7E and then never sees the CCC.
 *
 * m_stage is JTAG-visible progress; m_nacked collects per-phase NACK bits
 * (must stay 0). Exits 0 -> exc_signal 0xCAFEBABE.
 */

#include "ezh_test.h"

#define I3C0 0x40036000u
#define R(o) (*(volatile unsigned *)(I3C0 + (o)))
#define MCTRL 0x84u
#define MSTATUS 0x88u
#define MINTSET 0x90u
#define MINTCLR 0x94u
#define MWDATAB 0xB0u
#define MWDATABE 0xB4u
#define MRDATAB 0xC0u

#define ST_NACKED 0x20u
#define ST_MCTRLDONE 0x200u
#define ST_COMPLETE 0x400u
#define ST_RXPEND 0x800u
#define ST_TXNOTFULL 0x1000u

#define REQ_EMITSTART 1u
#define REQ_EMITSTOP 2u
#define REQ_PROCESSDAA 4u
#define REQ_FORCEEXIT 6u
#define TYPE_I3C (0u << 4) /* native I3C SDR (the I2C demos used 1<<4) */
#define ADDR(a) ((a) << 9)

#define CCC_RSTDAA 0x06u
#define DYN_ADDR 0x30u

/* Bitslice combiner encoding (see EVENT_FABRIC.md). */
#define BS0(c) ((c) << 8)
#define BS1(c) ((c) << 11)
#define BS2(c) ((c) << 14)
#define BS3(c) ((c) << 17)
#define BS4(c) ((c) << 20)
#define BS5(c) ((c) << 23)
#define BS6(c) ((c) << 26)
#define BS7(c) ((c) << 29)
#define BS_SIG 4u
#define BS_0 6u
#define REST_FORCE0                                                            \
  (BS1(BS_0) | BS2(BS_0) | BS3(BS_0) | BS4(BS_0) | BS5(BS_0) | BS6(BS_0) |     \
   BS7(BS_0))

volatile unsigned m_stage = 0, m_nacked = 0, m_holds = 0, m_complete = 0;
volatile unsigned m_pid_lo = 0, m_pid_hi = 0, m_bcrdcr = 0;

/* --- read-probe results (the actual experiment) ---------------------------
 * The question: when MSTATUS.COMPLETE first reads high to a polling master,
 * is MDATACTRL.RXCOUNT already guaranteed to reflect every received byte, or
 * can COMPLETE be observable a beat before the final byte's count settles?
 * We issue many N-byte SDR reads (N <= RX FIFO depth, so nothing is drained
 * mid-read), and for each read record RXCOUNT at the FIRST sample where
 * COMPLETE is high (r_first) versus RXCOUNT after it settles (r_final).
 *   r_race counts reads where r_first < r_final  ==>  the skew is real. */
#ifndef RDN_OVR
#define RDN_OVR 4u
#endif
#define RDN RDN_OVR                  /* bytes per read, <= 8-deep RX FIFO */
#define DIR_READ 0x100u           /* MCTRL.DIR = 1 (read) */
#define RDTERM(n) ((n) << 16)     /* MCTRL.RDTERM auto-terminate count */
#define RXCOUNT(v) (((v) >> 24) & 0x1Fu)
#define MDATACTRL 0xACu
volatile unsigned m_reads = 0;    /* completed N-byte reads */
volatile unsigned m_race = 0;     /* reads where COMPLETE preceded final count */
volatile unsigned m_minfirst = 99;/* smallest RXCOUNT ever seen at first COMPLETE */
volatile unsigned m_short = 0;    /* reads that did not deliver N bytes (skipped) */
volatile unsigned m_hist[6] = {0};/* histogram of RXCOUNT-at-first-COMPLETE (0..5) */
volatile unsigned m_last_first = 0, m_last_final = 0;
volatile unsigned m_race_pre = 0;   /* skew caught reading RXCOUNT BEFORE status */
volatile unsigned m_minpre = 99;    /* smallest RXCOUNT-before-status at COMPLETE */

static unsigned g_cfm;

/* Point the IRQ at `mask`, then sleep until MSTATUS shows any of it. */
static unsigned ev_hold(unsigned mask) {
  R(MINTCLR) = 0xFFFFu;
  R(MINTSET) = mask;
  __builtin_ezh_write_cfm(g_cfm); /* clear any stale sticky flag */
  unsigned st;
  while (((st = R(MSTATUS)) & mask) == 0u) {
    __builtin_ezh_hold();
    m_holds++;
    __builtin_ezh_write_cfm(g_cfm);
  }
  return st;
}

/* Emit a STOP if the FSM is not idle (from IDLE the request would be
 * ignored and the wait would hang). */
static void stop_if_active(void) {
  if ((R(MSTATUS) & 7u) != 0u) {
    R(MCTRL) = REQ_EMITSTOP;
    ev_hold(ST_MCTRLDONE);
    R(MSTATUS) = ST_MCTRLDONE | ST_COMPLETE | ST_NACKED;
  }
}

int main(void) {
  unsigned cfm = BS0(BS_SIG) | REST_FORCE0 | 0xFFu;
  g_cfm = cfm;
  __builtin_ezh_write_cfs(BS0(0u));
  __builtin_ezh_write_cfm(cfm);
  __builtin_ezh_write_cfm(__builtin_ezh_read_cfm());

  R(MSTATUS) = 0xFFFFFFFFu; /* clear stale W1C status */

  /* Bus hygiene: recover if a previous run left the FSM mid-message. */
  if ((R(MSTATUS) & 7u) != 0u) {
    R(MCTRL) = REQ_FORCEEXIT;
    ev_hold(ST_MCTRLDONE);
    R(MSTATUS) = ST_MCTRLDONE;
  }
  stop_if_active();

  /* --- RSTDAA broadcast: reset any stale dynamic address (NXP flow) ---
   * The CCC byte is PRELOADED so the message never stalls mid-flight. */
  R(MWDATABE) = CCC_RSTDAA; /* END -> message closes with STOP */
  R(MCTRL) = REQ_EMITSTART | TYPE_I3C | ADDR(0x7Eu);
  ev_hold(ST_COMPLETE);
  R(MSTATUS) = ST_COMPLETE | ST_MCTRLDONE | ST_NACKED;
  stop_if_active();
  m_stage = 10;

  /* --- ENTDAA (MCTRL.REQUEST = ProcessDAA): the hardware owns the bus
   * ceremony; the firmware is woken by RXPEND to collect the slave's 8 ID
   * bytes (48-bit PID + BCR + DCR), then by MCTRLDONE to write the chosen
   * dynamic address, then ProcessDAA again until COMPLETE. --- */
  unsigned id[8], got = 0u, assigned = 0u;
  R(MCTRL) = REQ_PROCESSDAA;
  for (;;) {
    unsigned st = ev_hold(ST_RXPEND | ST_MCTRLDONE | ST_COMPLETE);
    while (R(MSTATUS) & ST_RXPEND) { /* ID bytes arriving */
      unsigned b = R(MRDATAB) & 0xFFu;
      if (got < 8u)
        id[got] = b;
      got++;
    }
    if (st & ST_COMPLETE) { /* closing 0x7E NACKed: DAA is done */
      R(MSTATUS) = ST_COMPLETE | ST_MCTRLDONE | ST_NACKED;
      break;
    }
    if (st & ST_MCTRLDONE) { /* between DAA phases */
      R(MSTATUS) = ST_MCTRLDONE;
      if (got >= 8u && !assigned) {
        m_pid_lo = id[0] | (id[1] << 8) | (id[2] << 16) | (id[3] << 24);
        m_pid_hi = id[4] | (id[5] << 8);
        m_bcrdcr = id[6] | (id[7] << 8);
        R(MWDATAB) = DYN_ADDR; /* 7-bit address; hardware adds parity */
        assigned = 1u;
        got = 0u;
      }
      R(MCTRL) = REQ_PROCESSDAA; /* continue / close the DAA */
    }
  }
  stop_if_active();
  m_stage = 1;
  if (!assigned) { /* no slave took part: give up loudly */
    m_stage = 99;
    return 1;
  }

  /* --- read-probe: many N-byte SDR reads, capture RXCOUNT-vs-COMPLETE --- */
  m_stage = 2;
  for (unsigned it = 0u; it < 4000u; it++) {
    /* issue an N-byte read (repeated START each time) */
    R(MCTRL) = REQ_EMITSTART | TYPE_I3C | DIR_READ | ADDR(DYN_ADDR) | RDTERM(RDN);

    unsigned r_first = 0xFFu, r_pre = 0xFFu, seen = 0u, tail = 0u;
    for (unsigned spin = 0u; spin < 30000u; spin++) {
      unsigned rx_pre = RXCOUNT(R(MDATACTRL)); /* count BEFORE the status read */
      unsigned st = R(MSTATUS);
      unsigned rx = RXCOUNT(R(MDATACTRL));     /* count AFTER (engineer's order) */
      if ((st & ST_COMPLETE) && !seen) {
        r_first = rx;   /* what the engineer's flags-then-count read would see */
        r_pre = rx_pre; /* the widest window: count sampled a beat earlier */
        seen = 1u;
      }
      if (seen && ++tail > 12u)
        break;
    }
    unsigned r_final = RXCOUNT(R(MDATACTRL));

    if (seen && r_final == RDN) {
      m_reads++;
      if (r_first < 6u)
        m_hist[r_first]++;
      if (r_first < m_minfirst)
        m_minfirst = r_first;
      if (r_first < r_final)
        m_race++; /* engineer's order: COMPLETE seen before final byte counted */
      if (r_pre < r_final) {
        m_race_pre++; /* wider window: count one beat before COMPLETE was short */
        if (r_pre < m_minpre)
          m_minpre = r_pre;
      }
      m_last_first = r_first;
      m_last_final = r_final;
    } else {
      m_short++; /* underrun or no data this round -- ignore for the timing */
    }

    /* drain the FIFO and clear status for the next read */
    for (unsigned d = 0u; d < r_final; d++)
      (void)R(MRDATAB);
    R(MSTATUS) = ST_COMPLETE | ST_MCTRLDONE | ST_NACKED;
  }
  R(MCTRL) = REQ_EMITSTOP;
  ev_hold(ST_MCTRLDONE);
  R(MSTATUS) = ST_MCTRLDONE | ST_COMPLETE | ST_NACKED;
  m_complete = 1u;
  m_stage = 4;
  return 0; /* crt0 -> exc_signal = 0xCAFEBABE */
}
