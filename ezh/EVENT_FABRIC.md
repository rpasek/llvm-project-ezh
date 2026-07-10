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

# EZH event fabric — reverse-engineering findings

This documents what we learned driving the SmartDMA/EZH **event fabric**
(`e_hold` / `e_vectored_hold`, bit-slices, the logical combiner) on real
silicon (EVK-MIMXRT595), why it is **not** a speed lever for bit-banged I2C,
and the concrete backend facts the effort surfaced. For the ISA/core
background see [BACKGROUND.md](BACKGROUND.md); for the board/JTAG setup see
[README.md](README.md).

Primary ISA source: NXP **AN14650** (SmartDMA Cookbook). Encodings were
cross-checked against the silicon-derived `ezhdis` disassembler.

## TL;DR

* **`e_hold` genuinely blocks** the core until the logical combiner reads 1 —
  *with the right combiner config*. A bare `ezh_write_cfm(0)` does **not** zero
  the combiner (it reads 1); you must force all eight bit-slices to `BS_0` and
  OR-enable them. (An earlier conclusion that "`e_hold` doesn't block" was this
  config bug, now corrected.)
* **The full real-edge path works**: an M33-driven GPIO edge →
  INPUTMUX → SmartDMA trigger channel → CFS/CFM → bit-slice → combiner **wakes
  a blocking `e_hold`** on silicon (validated, [`ezh_test/ezh_realedge.c`](ezh_test/ezh_realedge.c)).
* **`vectored_hold` dispatch: SOLVED** (see the STAGE 2 addendum below). The
  arming is **inside the instruction word**: the ACC form's `vectors` imm8
  (bits[31:24]) is a per-slice dispatch-enable mask, bits[13:10] select the
  register that *receives* the vector (`table + 4 + 4*slice`; PC = hardware
  GOTO), and bits[23:20] the table base. The **plain form encodes an empty
  mask and can never dispatch** — that (not missing M33 boot state) is why
  it always resumed inline. Auto-dispatch validated on silicon:
  [`ezh_test/ezh_vhold_dispatch.c`](ezh_test/ezh_vhold_dispatch.c).
* **The event fabric is the wrong lever for I2C speed**: ~6-cycle wake loses to
  the 3-instruction GPI poll, and NXP ships the path disabled. The real I2C
  bit-bang ceiling (~410 kHz) is set by GPI/GPD throughput at the (already
  maxed) 216 MHz core clock.

## The fabric, end to end

```
 pin / IRQ                 on-chip routing                   EZH core
 ─────────      ┌───────────────────────────────────┐    ┌────────────┐
 PIO1_0  ──▶ INPUTMUX ──▶ SmartDMA   ──▶ bit-slice n ─▶ logical ─▶ e_hold
 (GPIO)      trig ch.        channel        (CFS src,     combiner    wakes
                                             CFM mode)    (OR of 8)
```

* **INPUTMUX** binds a Port0/Port1 GPIO (or an IRQ line) to a SmartDMA trigger
  channel: `SMART_DMA_TRIG_CH_SEL[n]` at `0x40026720 + n*4` (write the source
  selector, e.g. `8` = `GpioPort1Pin0ToSmartDmaInput`).
  **Gotcha:** the INPUTMUX clock must be enabled first —
  `CLKCTL1 PSCCTL2_SET` `0x40021048` bit31. The SDK's `INPUTMUX_Init` does
  this; our pokes had to add it.
* **CFS** (EZH special reg 10, 3 bits/slice) selects which channel feeds each
  slice. Exposed as `ezh_write_cfs` / `__builtin_ezh_write_cfs`.
* **CFM** (EZH special reg 11): 3 bits/slice detect *mode* + low byte = OR
  *enable* mask. Modes (AN14650 Table 10): `001`=`BS_RISE`, `010`=`BS_FALL`,
  `011`=`BS_CHANGE`, `100/101`=level (`BS_SIG`), `110`=`BS_0` (force 0),
  `111`=`BS_EVENT` (non-sticky). Writing CFM also clears a slice's sticky flag.
* Slice inputs come **only from Port0/Port1 GPIO** (via INPUTMUX), **not** the
  Port2 / J18 header pins — so the two-board J18 I2C edge can't drive a slice;
  a real edge must be an on-chip Port0/1 GPIO (e.g. M33-driven).

### Making the combiner actually 0 (so a hold blocks)

```c
#define REST_FORCE0 (BS1(BS_0)|BS2(BS_0)|BS3(BS_0)|BS4(BS_0)| \
                     BS5(BS_0)|BS6(BS_0)|BS7(BS_0))
ezh_write_cfs(BS0(EZH_INPUT_SOURCE_0));          /* slice0 <- channel 0   */
ezh_write_cfm(BS0(BS_SIG) | REST_FORCE0 | 0xFFu);/* slice0 active, rest 0 */
ezh_write_cfm(ezh_read_cfm());                   /* clear stale BS flags  */
/* combiner is now 0 -> e_hold BLOCKS until slice0's input goes high */
```

`BS_SIG` (level) reliably wakes on a slow M33-driven edge; sticky `BS_RISE`
can miss it (the edge predates the arm). For a fast hardware edge either works.

## STAGE 1 — real-edge wake (validated)

[`ezh_test/ezh_realedge.c`](ezh_test/ezh_realedge.c) +
[`ezh_test/ezh_realedge.py`](ezh_test/ezh_realedge.py): the M33 (over OpenOCD —
its AHB writes stand in for an M33 application) routes PIO1_0 → trig ch0, boots
the EZH firmware with the full `fsl_smartdma` sequence (RSTCTL reset; CTRL
`0xC0DE0010` install; ARM2EZH/BOOTADR; CTRL `0xC0DE0011`), the EZH arms slice 0
and `e_hold`s **blocked**, then the M33 drives PIO1_0 high and **the hold
wakes** (`exc_signal` → `0xCAFEBABE`). The INPUTMUX→channel→slice→combiner path
works with a genuine pin edge — which the PENDTRAP software doorbell could not
provide for dispatch.

## STAGE 2 — `e_vectored_hold` resumes inline (does not dispatch)

AN14650 describes `e_vectored_hold` as halting then `GOTO table[base+4+slice]`.
**On our silicon, in this harness, that dispatch never fires.** Measured with
an explicit self-spin table plus a known inline marker:

```
241007a4: e_vectored_hold r6     ; hold (base r6 = 0x24100348, poked-reliable)
241007a8: e_load_imm  r5, 493    ┐ inline post-hold code
241007ac: e_str       r5, r4, 0  │  (would set exc = 0x1ED)
241007b0 <inl>: e_gotol inl      ┘ inline self-spin
        table lives at 0x24100348 … 0x24100748
```

After the real edge: **PC = `0x241007b8`** = the inline `inl` spin (`0x7b0`) +
the SmartDMA PC register's ~8-byte read-ahead. PC **never entered the table**;
the hold resumed ~2 instructions past itself (the scheduled shadow slots / RA
resume). The bit-slice combiner supplies the **wake** but **no per-slice vector
index**, so the dispatch path is not taken — neither the real GPI edge nor the
PENDTRAP doorbell presents a vector.

**The earlier "garbage dispatch targets" were not dispatches.** Values like
`0x20000070`, `&vh_base`, and literal-pool addresses were the EZH resuming
inline and then running off the end of an `-Os` `for(;;)` into the literal pool,
executing **data words as instructions** and wandering. With a clean inline
self-spin the wandering stops and the inline resume is unambiguous.

To actually fire the table dispatch you need whatever arms the vector index —
almost certainly the full M33 `fsl_smartdma` boot state (or a specific
vector-enable mode / the `_ACC`/`_NRA` variant), which the minimal
`BOOTADR+CTRL` ignite does not establish. See
[`m33_vhold_harness/`](m33_vhold_harness/) for the M33-side sketch that would
unblock the measurement.

### STAGE 2 addendum — SOLVED: the dispatch arming is inside the instruction

A later campaign (documented result:
[`ezh_test/ezh_vhold_dispatch.c`](ezh_test/ezh_vhold_dispatch.c) +
`ezh_vhold_dispatch.py`, all facts silicon-proven with a doorbell-driven
discrimination matrix on the 0x1C opcode's raw encodings) resolved this
completely. The paragraph above was on the wrong track: **no M33 boot state is
involved** (NXP's full driver boot writes nothing vector-related, and all four
shipped production firmware blobs contain 234 plain holds and zero vectored
holds). The true contract of the HOLD-family word:

* **bits[13:10] — `rDest`**: the register that *receives the vector*. On a
  dispatching wake the hardware writes `rDest := rTable + 4 + 4*slice`
  (AN14650's `base+4+slice`, stride 4). With `rDest = PC` (0x0D) the write
  *is* the dispatch — a hardware GOTO with no jump instruction (plain `hold`
  is exactly this shape with bit15 set: "write resume to PC" = inline resume).
* **bits[23:20] — `rTable`**: the jump-table base register. (NXP's
  `E_VECTORED_HOLD` macros put the *same* register in both fields, which
  obscures the two roles; with distinct registers the mechanism is plain.)
* **bits[31:24] — `vectors`**: the per-slice **dispatch-enable mask**, with
  bit9 = the ACC form. A wake won by slice *n* dispatches only if mask bit
  *n* is set; a masked-out slice still *wakes* the hold but resumes inline —
  a deliberate spurious-wake path, not a failure.
* **The plain `vectored_hold` encodes `vectors = 0` and therefore can never
  dispatch.** Every earlier "no dispatch" observation — ours and, evidently,
  NXP's own unused macro — was this by-design empty mask.

Measured proof: two slices armed (`BS_EVENT` detect, SmartDMA trigger
channels 0/2 fired via the PENDTRAP software doorbell `EN(n)|REQ(n)` at
`0x40027048`); auto-dispatch encoding `0xFF60361C` (rDest=PC, table=r6,
mask=0xFF) landed a 256-slot counting table at **slot 1 for slice 0 and slot
3 for slice 2** — `table + 4 + 4*slice` exactly, no jump instruction executed.
Masking to `VECT0` only: slice 0 still dispatches, slice 2 wakes inline.

Backend follow-up this implies: the `VECTORED_HOLD`/`ACC_VECTORED_HOLD` td
defs model a single `$table` operand feeding both register fields (the NXP
macro shape, which cannot dispatch usefully with `rDest = rTable` unless
intended); the honest model is two operands (`rDest`, `rTable`) — and with
the semantics now proven, a C surface becomes possible: an intrinsic
returning the vector address (GPR `rDest`), dispatched with a computed goto.

## Backend / toolchain facts nailed down

These are concrete and reusable regardless of the fabric:

* **`e_load_imm` takes an 11-bit *signed* immediate (−1024 … 1023).** This is
  why `e_load_imm rX, <32-bit symbol>` cannot load an address — the value does
  not fit. A numeric out-of-range operand is correctly rejected
  (`immediate operand 4660 is out of range … requires 11-bit signed`), **but a
  symbol/relocation operand is silently mis-assembled** (the address word is
  dropped inline as a bogus instruction). To materialize a 32-bit constant or
  address, pass it as a C operand so the compiler emits a PC-relative literal
  load (`e_ldr rX, pc, off`); do not hand-write `e_load_imm rX, symbol`.
  *(Backend bug candidate — see below.)*
* **The EZH data directive is `.long`**, not `.word` (the EZH MC backend
  rejects `.word` as an unknown directive).
* **SmartDMA SRAM lives only at `0x24100000`** — the EZH fetches *and*
  reads/writes data there. `0x20000000` is the **M33 system SRAM**, not an
  alias of the EZH SRAM (verified: different contents).
* **Reading where the EZH landed:** poke the table/handler base from a global
  (`e_ldr r6, &g, 0; e_vectored_hold r6`) for reliability, fill the target with
  **explicit-label** self-spins (`s0: e_gotol s0` …; a bare `.`-relative
  `e_gotol .` does **not** self-spin, it falls through), then read the SmartDMA
  PC register `0x40027028` (which reads ~8 bytes ahead of the true PC).
* **Probe note:** heavy repeated probing wedges the LPC-LINK2 USB
  (`CMD_CONNECT` fails); recover with `pkill -f openocd` + restart.

## Backend implications (open items)

1. **`e_load_imm` with a symbol/reloc should be a hard error, not silent
   garbage.** The 11-bit-signed field cannot hold a 32-bit fixup; the assembler
   already diagnoses out-of-range *numeric* immediates, so the symbol path is an
   asymmetry worth closing in the EZH MC layer (`EZHAsmParser` /
   `EZHMCCodeEmitter` fixup range check). Optionally, teach the assembler to
   auto-lower an oversized `e_load_imm` into a PC-relative literal load (what the
   compiler already does for C-level constants).
2. The conditional/vectored hold defs (`HOLD#cc`, `VECTORED_HOLD`,
   `ACC_VECTORED_HOLD`) remain **assembler-only** (empty ISel patterns) — the
   right C surface for a *dispatching* hold is `asm goto`/`callbr` listing the
   per-slice handler labels, not a plain intrinsic. Only worth building once a
   full M33-boot harness proves the dispatch fires.
3. `ezh_write_cfs` (added this round) closed the scheduler-visibility gap for
   CFS writes; a matching helper for the GPISYNCH/INPUTMUX startup (only needed
   for live-pin wake) is still out of tree.

## Bottom line

The EZH is already at full clock (216 MHz); the 3-instruction GPI poll is
optimal; ~410 kHz (I2C fast-mode) is the genuine bit-bang ceiling. The event
fabric helps with **sparse wake-from-idle** (a blocking hold woken by a real
edge — validated here), not peak bit-bang throughput. For MHz-class I2C use the
chip's FlexComm-I2C / I3C peripheral on the M33, not the EZH.
