# tight_loop body-shape timing on RT595 silicon

Measured 2026-07 with `ezh_test/ezh_tightloop_timing.c` /
`ezh_test/run_tl_timing.sh`: the EZH core times its own hardware loops
against CTIMER0 free-running on FRO_DIV1 (the tick-per-cycle quantum cancels
in ratios; `q = (alu2 - alu1) / iters` came out 0.885 and dead stable). Every
number below survived two independent validity gates: a static
instruction-by-instruction check of each emitted `[tight_loop; nop; body...]`
sequence, and a per-case ARCHITECTURAL-state check over patterned data —
pointer march distances, exact accumulator totals, and destination contents.
The rotated cases pin the cross-wrap dependency by value, not just by time:
`load_rot`'s accumulator total is only right if each `add` consumed the word
loaded one iteration earlier across the wrap, and `copy_rot`'s destination
must be the source shifted by exactly one byte with `dst[0]` carrying the
previous pass's final load. Both gates caught real bugs before any number
was trusted — see "measurement war stories" below.

## The question: is loop rotation worth implementing?

For a two-instruction pump body `[ldr_post; add]` the load-use pair sits at
issue distance 1 inside the repeated block. Rotating the body to
`[add; ldr_post]` (run-once slot holding the first load, epilogue consuming
the last) moves the same pair to distance 1 *across* the hardware loop-back.
Rotation therefore pays only if the wrap adds a hidden cycle or the
interlock loses track of values across block re-entry. AN14650 answers
neither. Silicon does:

## Results (cycles per iteration, 100,000 iterations per case)

| case        | body                                | cycles/iter |
|-------------|-------------------------------------|-------------|
| alu1        | `add_imm`                           | 1.003       |
| alu2        | `add_imm x2`                        | 2.003       |
| alu3        | `add_imm x3`                        | 3.003       |
| nop1        | `nop`                               | 1.003       |
| load_use    | `ldr_post; add` (use at dist 1)     | 4.004       |
| load_rot    | `add; ldr_post` (use across wrap)   | 4.003       |
| load_space  | `ldr_post; add_imm; add` (dist 2)   | 4.005       |
| load_only   | `ldr_post`                          | 2.003       |
| store_only  | `str_post`                          | 3.004       |
| copy        | `ldrb_post; strb_post`              | 5.005       |
| copy_rot    | `strb_post; ldrb_post`              | 5.004       |
| load2       | `ldr_post; ldr_post` (independent)  | 4.006       |
| fill_sw     | ordinary `str_post; dec; goto_nz`   | 7.002       |
| sum_sw      | ordinary `ldr_post; add; dec; bnz`  | 8.002       |
| copy_sw     | ordinary 4-insn copy loop           | 9.003       |
| perload     | `ldr` from CTIMER TC (APB)          | 9.136       |

## Established facts

- **The hardware loop-back is free**: w = 0.003 ≈ 0 cycles. alu1/2/3 are
  perfectly linear at 1 cycle per ALU op; `nop` is 1 cycle.
- **Rotation is worthless**: s_in = s_wrap = 2.00 exactly. The interlock
  tracks the pending load across block re-entry with zero difference, and
  there is no hidden wrap cycle to hide it under. `copy_rot == copy` confirms
  the same for store-consumer pairs. **EZHTightLoopFormation deliberately
  does not implement rotation** (and the run-once slot keeps its `nop`: the
  slot runs once per loop *entry*, so filling it could only ever save a
  single cycle, at the price of an epilogue clone plus an N>=2 guard).
- **SRAM load shape**: a load occupies the core for 2 issue slots
  (`load_only` = 2, independent `load2` = 4) and its data is usable 3 cycles
  after issue start. Consequence: ONE independent instruction after a load
  is free (absorbed by the stall — `load_space` = 4 = `load_use`); the
  in-order core cannot do better than 4 cycles for a 2-instruction load-use
  pump no matter the arrangement.
- **The post-RA scheduler's LoadLatency=2 encoding is exactly right** by a
  happy cancellation: the scheduler models a load as 1-slot issue +
  latency 2, so it prefers one filler between load and use — and one filler
  is precisely what silicon absorbs for free. Modeling the "true" 3-cycle
  data latency would make the scheduler chase a second filler for zero gain.
- **Stores drain at 1 per 3 cycles** back-to-back (`store_only` = 3, and
  `fill_sw` = 3 + 4 branch overhead). Spacing independent work between
  stores could hide the drain, but the post-RA list scheduler can only see
  that through an itinerary-based hazard recognizer — and itineraries
  measurably hurt this backend elsewhere (see EZHSchedule.td). Documented,
  not modeled.
- **A taken `goto_nz` costs 3 cycles** (sw-loop minus tight_loop deltas are
  exactly 4 = 1 for the decrement + 3 for the branch), consistent with
  AN14650's "2-slot scheduled branch".
- **tight_loop's real win over software loops**: fill 7 -> 3 (2.3x),
  sum 8 -> 4 (2.0x), copy 9 -> 5 (1.8x) cycles per element.
- **Peripheral reads cost ~9.1 cycles** (CTIMER on the APB bridge; the
  non-integer ratio is the asynchronous clock-domain crossing). Polling a
  peripheral FIFO status register is ~4.5x the cost of an SRAM load —
  budget accordingly in pump loops.

## Measurement war stories (why the validity gates exist)

1. The scheduler placed pointer-reset instructions between the intrinsic
   and the body — inside the repeated region. Caught by the static check;
   fixed with explicit register-pinning asm barriers (`PIN*` macros).
2. The constant-island pass placed an island plus branch-around `goto`
   between a one-instruction body and its Rend label. The taken `goto`
   inside the repeated region aborted the loop after one iteration —
   `store_only` read 0.008 cycles/iter. This was a REAL COMPILER BUG for
   hand-written `__builtin_ezh_tight_loop` loops (formation-created loops
   were already protected): fixed by registering every block containing a
   TIGHT_LOOP as a no-water zone in EZHConstantIslandPass; regression test
   `llvm/test/CodeGen/EZH/tight-loop-intrinsic-island.ll`.
3. The byte loads' asm operand order is DATA-FIRST (`ldrb_post data, ptr,
   step`), unlike word loads and all stores which are pointer-first
   (`ldr_post ptr, data, step` / `str_post ptr, data, step`). Hand-written
   asm using pointer-first byte loads marched the data register through low
   memory instead; caught by the architectural-state check (the "copy"
   numbers in an earlier run were really measuring a writeback-consumer
   pair — 4.005, not the true 5.005).

## Hand-written __builtin_ezh_tight_loop checklist

- Pin every register the body consumes with a volatile asm barrier BEFORE
  the intrinsic, or the compiler may sink setup into the repeated region.
- Put the whole body in ONE volatile asm block; give the run-once slot an
  explicit `nop`.
- The compiler cannot guarantee `[tight_loop .. &&label)` stays straight
  line at the IR level (operand materializations may land inside); verify
  the disassembly, instruction by instruction, before trusting behavior.
- Byte loads are data-first; word loads and all stores are pointer-first.
