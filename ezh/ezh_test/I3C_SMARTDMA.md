<!--
Copyright 2026 Google LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# HW-I2C via SmartDMA, and the literal `tight_loop`

Instead of bit-banging I2C on the EZH (see the `ezh_i2c_*` tests, which top out
~410 kHz), this drives the on-chip **I3C0** peripheral in **legacy-I2C master
mode** and uses the EZH/SmartDMA as its data engine. I3C0 generates START,
address, ACK sampling, byte timing and STOP; the EZH just feeds/paces the FIFO.
It runs over the *already-wired* two-board I3C header — `PIO2_29 = I3C0_SCL`,
`PIO2_30 = I3C0_SDA` — the same pins the bit-bang slave reads as `SMARTDMA_PIO`.

Run it: `./run_i3c_smartdma.sh` (board A `:4444` = master, board B `:4445` =
`ezh_i2c_slave_regmap` slave). Three stages, all validated on an EVK-MIMXRT595:

| stage | firmware | what it shows |
|-------|----------|---------------|
| M0b | [`i3c_hw_master.c`](i3c_hw_master.c) | EZH drives the I3C0 master, 5-byte poll write |
| M1  | [`i3c_stream.c`](i3c_stream.c) | event-paced streaming of 16 bytes (2× the FIFO) via `__builtin_ezh_hold` |
| M1b | [`i3c_tight_loop.c`](i3c_tight_loop.c) | the **literal `tight_loop` (OP_LOOP)** hardware loop |

Why I3C0 and not FLEXCOMM: I3C0 (`0x40036000`) sits **inside** the EZH
`per_read`/`per_write` window (`0x40000000`–`0x400FFFFF`), so the EZH reaches it
with the optimized single-instruction peripheral access; it has a TX/RX FIFO;
and it routes to the SmartDMA event fabric. FLEXCOMM is outside the window and
would have needed re-wiring.

## Master bring-up (M33 side) — the one non-obvious step

`run_i3c_smartdma.sh:i3c_init()` does the whole bring-up. Register-for-register
it mirrors NXP's `I3C_MasterInit`, and the step a naïve raw-poke path skips is
the **peripheral-reset pulse**:

```
RSTCTL1.PRSTCTL2_SET (0x40020048) = 1<<16   # assert I3C0 reset
RSTCTL1.PRSTCTL2_CLR (0x40020078) = 1<<16   # deassert -> bus FSM initialises
```

I3C0 is *not* held in reset (the bit reads 0), but without the assert→deassert
pulse the bus state machine will not emit a START: `EmitStartAddr` silently
no-ops (STATE stays IDLE, no `MCTRLDONE`, no error), while `EmitStop` /
`IbiAckNack` *do* move the FSM — which misleads you into blaming the clock, the
pull-ups, or `MSTENA`. The pulse both arms the START engine and clears the
power-on phantom-IBI, giving a clean `STATE=0` (IDLE). The rest:

- **Clocks (CLKCTL1 `0x40021xxx`):** gate `PSCCTL2` bit16; functional clock
  `FCLKSEL=FRO_DIV8`, `FCLKDIV` div1 (reset-pulse it); timing-control clock
  `STCSEL/STCDIV` derived from the functional clock. `FRODIVOEN` (`0x40001110`)
  enables the FRO divided outputs first.
- **MCONFIG (`0x40036000`):** `MSTENA` (0b01 = MASTER_ON), `DISTO`, `ODSTOP`,
  `ODHPP` (actively drives the bus high), `SKEW=1` (I2C errata), + baud ~100 kHz.
- **Pins:** `IOPCTL PIO2_29/30 = 0x71` (FUNC1 + pull-up + input buffer, drain
  *disabled* — the peripheral manages open-drain), `PIO2_31 = I3C0_PUR`.
- **FIFO:** `MDATACTRL = 0x3B` (UNLOCK | TX-trigger | flush both).

Transaction from the EZH: clear `MSTATUS`; `MCTRL = EmitStartAddr | TYPE=I2C |
DIR=W | ADDR<<9`; poll `MCTRLDONE`; push bytes to `MWDATAB`, last via `MWDATABE`
(appends STOP); poll `COMPLETE`.

## Event pacing (M1) — feeding more than the FIFO holds

To stream 16 bytes through the 8-deep FIFO, each write is gated by
`__builtin_ezh_hold()`, which blocks the core until the I3C0 "TX not full"
event, routed:

```
I3C0 MINTSET.TXNOTFULL -> IRQ
  -> INPUTMUX SMART_DMA_TRIG_CH_SEL[0]=26 (I3c0Irq) -> SmartDMA trig ch 0
    -> bitslice 0 (CFS=ch0, CFM=level) -> logical combiner -> hold wakes
```

`TXNOTFULL` is a **level**, so after each byte the firmware re-writes `CFM` to
clear the sticky bitslice flag; otherwise the stale level double-wakes the next
hold and two bytes land in one freed slot (overflow → `OWRITE`, first 8 bytes
correct then every other one drops). With the per-byte clear it is exactly one
write per drained slot, and all 16 bytes arrive byte-exact. (Event fabric
details: [`../EVENT_FABRIC.md`](../EVENT_FABRIC.md).)

## The literal `tight_loop` (M1b)

`tight_loop <Rend>, <Rcount>` is the EZH's zero-overhead hardware loop. Both
operands are **registers**:

```
tight_loop Rend, Rcount     ; Rcount = (iterations) - 1
  <single straight-line body, no internal branches>   ; runs Rcount+1 times
Rend:                       ; execution falls through to this address after
```

Validated in `i3c_tight_loop.c` (Rcount=6 → **exactly 7 iterations**, body =
`hold; ldrb_post r4,r0,1; per_write r4,MWDATAB; mov cfm,r2`, slave got
`0x10..0x16` byte-exact, COMPLETE, no error). Two things make it work:

1. **`Rend` is a code address after the body.** Materialising one in hand asm is
   the classic trap (`ldr rX, pc, <lit>` read back 0 at runtime). The compiler
   gets it right: `__builtin_ezh_tight_loop(&&loop_end, n-1)` turns the
   label-as-value into a blockaddress and emits a correct PC-relative
   literal-pool load, and register-allocates both operands.
2. **The body must be a single basic block** and is meant to `hold` on a
   peripheral event each iteration (why NXP's SmartDMA bodies always hold).
   Because the compiler models the body as straight-line code that runs once,
   every body operation must be side-effecting (EZH intrinsics / volatile
   accesses) and loop-carried state must live in volatile storage — see the
   body contract in `IntrinsicsEZH.td` and the `vptr` idiom in the test.
3. A silicon bonus fact from the builtin run: the compiler reused the Rend
   register inside the body and the loop still iterated correctly — the
   hardware **latches** Rend/Rcount at loop entry (NXP's firmware relies on
   the same).

### The compiler surface: a builtin, not a loop transformation

`tight_loop` is exposed as `__builtin_ezh_tight_loop(rend, rcount)` — it emits
exactly the one instruction. What the backend deliberately does **not** do is
convert counted C loops into `tight_loop` automatically (a HardwareLoops-style
optimization), because that would change behaviour, not just speed:

- A **free-running** `tight_loop` (no armed fabric, no hold) runs its body
  **zero** times on silicon — a transparently converted plain loop would
  miscompile.
- The `hold` **inside** `tight_loop` does **not** cleanly pace a *level*
  peripheral event the way a plain hold in a C loop does. Streaming > 8 bytes
  with `tight_loop` overflows: the 2-slot scheduled loop-back's delay slots +
  the level `TXNOTFULL` + the per-iteration `CFM` clear don't compose into
  one-write-per-slot (this test streams ≤ 7 bytes so the count semantics are
  unambiguous). The plain C loop in `i3c_stream.c` paces the same level event
  perfectly.
- Its pacing event is hardware-fabric state (INPUTMUX/CFS/CFM/peripheral
  interrupt routing) the compiler has no model of, and the loop-branch it
  removes costs nanoseconds — only relevant for MHz-class event streams
  (e.g. FLEXIO display DMA) that are hand-written anyway.

So the boundary is: ordinary C `for`-loop + `__builtin_ezh_hold` for robust
event-paced streaming; `__builtin_ezh_tight_loop` for the expert hand-tuned
MHz-class case — the whole ISA reachable from C, no inline asm required.

## M2 — fully interrupt-driven, both directions (`run_i3c_irq.sh`)

`i3c_slave_irq.c` closes the loop: board B now runs the I3C0 block in
**I2C-legacy slave mode** (static address 0x42, same J18 pins) with the EZH
asleep in `__builtin_ezh_hold()` between bus events. The slave's
`SINTSET.{RXPEND,STOP}` interrupts ride the same path the master's TX pacing
uses (I3C0 IRQ -> INPUTMUX ch0 -> bitslice 0 -> combiner), so with the
event-paced `i3c_stream.c` master on board A **neither side polls its data
path**. Silicon result: 17/17 bytes, STOP caught, slave woke once per event
(18 wakes incl. one tolerated spurious arm-time wake), both cores exit
0xCAFEBABE.

Why not an interrupt-driven *bit-bang* slave: the event fabric's slice inputs
are Port0/1 GPIO and IRQ lines only -- the J18 pins are Port2, so their edges
can never reach the combiner (see EVENT_FABRIC.md). Routing the peripheral's
IRQ is the correct idiom.

## M3 — true I3C SDR with ENTDAA, interrupt-driven both ends (`run_i3c_sdr.sh`)

`i3c_sdr_master.c` + `i3c_sdr_slave.c` run native I3C (not I2C-legacy): RSTDAA
broadcast, then the real ENTDAA ceremony -- the master collects the slave's 8
DAA ID bytes (PID incl. our programmed part-no 0xCAFE1234, BCR, DCR) and
assigns dynamic address 0x30 -- then a 17-byte private SDR write at ~12 MHz
push-pull. Both EZH cores sleep in `__builtin_ezh_hold()` for every phase
(master: MCTRLDONE/RXPEND/COMPLETE/TXNOTFULL via MINTSET switching; slave:
DACHG/RXPEND/STOP). Silicon result: DA assigned (SDYNADDR 0x61), 17/17 bytes,
both cores exit 0xCAFEBABE.

Hard-won bring-up facts (each cost a debugging round on silicon):

* **The CCC/DAA engine runs off the I3C time-control (slow) clock** -- LPOSC
  1 MHz via CLKCTL1 `I3C0FCLKSTCSEL=1`, with BOTH the TC and SLOW dividers
  unhalted (`0x40021808`/`0x4002180C`). Without it, repeated-START requests
  lodge in MCTRL forever and every CCC is silently ignored. SCONFIG.BAMATCH
  counts this clock: 1 MHz -> BAMATCH=1 (SDK formula, clamped).
* **Pin drive strength gates CCC/DAA specifically**: IOPCTL 0x1C1 (full drive,
  slew, no internal pulls) on SCL/SDA and 0x181 on PUR. With the weak 0x71
  config, address match and private SDR writes still work -- but the
  PUR-dependent open-drain turnarounds corrupt, so the slave ACKs 0x7E and
  never sees the CCC byte. Diagnosed by diffing the live registers of NXP's
  working `i3c_interrupt_b2b` example running on the same boards.
* **MCTRL requests race at EZH speed**: a new request written ~1 us after the
  previous phase lodges but never executes. Let the TX FIFO drain
  (MDATACTRL[20:16]==0) plus a ~1 ms settle before the next request.

The M33-side bring-up (clocks, reset pulse, pins, SCONFIG/SMAXLIMITS) is in
`run_i3c_sdr.sh`.

### Board gotcha: never fully erase the flash

A COMPLETELY blank RT595 (mass-erased flash) is un-attachable by OpenOCD: the
ROM finds no image and enters deep sleep with the debug port unpowered
("Target not examined yet"). Recovery needs NXP LinkServer (its connect script
wakes the part via the debug mailbox) or ISP-mode straps. Any valid app in
flash -- e.g. the factory demo an EVK ships with -- keeps the part awake and
debuggable, and all these demos fully re-initialize the I3C block anyway, so
whatever is in flash is harmless. Also: prefer `reset halt` over `reset run`
when poking blank-ish boards over JTAG.

## M4 — two-source vectored dispatch, "the EZH's NVIC" (`run_i3c_vectored.sh`)

`i3c_vectored_slave.c`: one EZH core serves two interrupt sources concurrently
via `__builtin_ezh_acc_vectored_hold` -- slice 0 = the I3C0 IRQ (the full M3
SDR slave: DACHG/RXPEND/STOP), slice 1 = a software doorbell (PENDTRAP,
standing in for a display/tiling trigger) that copies 16-byte tiles. The
hardware computes the winning source's vector (base + 4 + 4*slice); the loop
derives the slice and runs the matching C handler. Board A runs the unchanged
M3 master. Silicon result: 22 hardware-vectored I3C wakes + 3 tile wakes, zero
spurious, 17/17 bytes + DA 0x61 + all tiles byte-exact, both cores 0xCAFEBABE.

Facts this demo surfaced:

* **The base vectored holds hardware-write RA** (the resume address --
  NVIC-style handler linkage; the `_nra` variants opt out). This was an
  unmodeled compiler clobber until now (fixed with `Defs=[RA]`): without it,
  returning from a function that used the builtin jumped back into the loop.
* **PENDTRAP REQ latches**: re-firing the same channel needs REQ toggled low
  first to make a fresh rising edge (a real peripheral trigger produces fresh
  edges by itself).

### Vectored-hold arbitration, measured (`run_vh_prio.sh`)

With two armed slices both pending at the moment `acc_vectored_hold` samples
(fired A-then-B, B-then-A, and both in one register write):

* **The lowest slice index wins** -- fixed priority, arrival order irrelevant
  (like the NVIC's exception-number priority).
* **A dispatch does not consume the sticky flags.** Every latched flag
  persists until the next full CFM write, so without a re-arm the winner just
  re-dispatches on every subsequent hold. The full-CFM re-arm is what clears
  -- and it clears ALL slices at once.

Design consequence for mixed sources: put must-not-miss pulse sources on low
slice numbers (they win and get serviced first), and within one wake service
the winner, then check the other sources' own status registers before the CFM
clear; level-type sources (like the I3C IRQ) are immune since they re-wake as
long as their line is high.
