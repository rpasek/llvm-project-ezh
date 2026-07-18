# EZH backend — upstream PR series plan (v2)

Supersedes the v1 note (which described the first 8-commit stack; that stack
was never merged and is now the base of this branch). Current state:
`ezh-port` carries **110 work commits over `origin/main` (`cf402fe`)** plus
planning commits like this one, all silicon-validated. Net compiler payload: **82 files, +8,690 / −734** across
`llvm/`, `clang/`, `lld/`, `lldb/` including **37 new in-tree test files**.
The other ~58 files (demos, JTAG harness, timing docs, 29 validation-artifact
commits) are fork-side evidence, not PR payload.

## Validation baseline every series inherits

- Full llvm-test-suite on EVK-MIMXRT595 silicon over JTAG: **4,617/4,617**
  across three configs — O0, Os, and `-O2 -mno-ezh-bitslice-interrupts`
  (the formation/full-opt config), including **127 organically formed
  tight_loop hardware loops across 107 binaries**.
- 41 portable compiler tests (hermetic runner, `ezh/check_ezh.py`), 37
  in-tree lit files, 16/16 JTAG sample suite, cycle-exact timing harness.

## Ground rules

1. **Carve the net diff, not the history.** The branch's audit-response and
   fix-forward commits collapse into their features; superseded work (e.g.
   the pre-scheduler no-post-RA pin, later replaced by the scheduler series)
   simply disappears from the net carving.
2. **Every series tip must**: build clean (`-Werror`), pass all in-tree EZH
   lit tests present at that point, pass the hermetic portable runner ON THE
   PER-SERIES MANIFEST (the full 41-test manifest hardcodes tests that later
   series introduce and reports missing files as failures -- each series
   carries the subset of the manifest that exists at its tip), and get one
   full three-config silicon suite run. Bisectability is the promise
   reviewers pay for.
3. Tests travel with their feature. Validation artifacts stay on the fork;
   PR descriptions summarize the evidence and link the fork commits.
4. Keep the base's conventions throughout (bare mnemonics, `GPR`,
   `pred:$Cond`, `EM_EZH = 14650`) — already true of every commit here.

## The series (dependency order)

| # | Series | Net content | Source commits (fork) |
|---|---|---|---|
| 1 | **Build & hygiene** | PassManagerBase forward-decl fix; `-Werror`-clean backend and lldb plugin; dead STATISTIC removal; predicated pushd/popd alias printing | `7e766844`, `3c233b24`, `e4ed4d91`, `932b3386`, `13d76eb0` |
| 2 | **MC diagnostics** | Silent mis-assembly → hard errors: symbol-immediate window, branch/call alignment, imm5 range; negative MC tests | `9b093d5f` |
| 3 | **Memory correctness** | Register-offset load/store ISel + the silicon-derived `Inst{29}` sign-extend fix; MachineMemOperands on indexed/spill accesses; NXP-confirmed R0-R7 operand enforcement | `00b6281c`, `cd3f47b1`, `e3eb277a` |
| 4 | **Intrinsic/builtin surface** | Full `__builtin_ezh_*` layer: GPIO, event fabric, CFM/CFS, GPO, vectored holds (incl. the hardware-writes-RA `Defs=[RA]` fact), `tight_loop`; the `getArchTypePrefix` fix that unblocks all of it; Sema immediate ranges; the silicon-corrected public contract (the IntrinsicsEZH/BuiltinsEZH rewrite from `ad00de20` -- ship the TRUE semantics from the start, never the disproven event-paced text) | `8753a819`, `7fd4b74f`, `1dd6cd5b`, `009c6d35`, `1b04f9e3`, `4ecd0901`, `2307e872`, `c22824c4`, `149a0350`, contract part of `ad00de20` |
| 5 | **Codegen quality I** | Frame elision (no phantom FP), EZHCompareFusion (+ -g/optnone hardening), constant-multiply decomposition + shift-add chains, sign-bias compare fold, 0/1 boolean contents | `d075e8b0`, `f4f39ab9`, `934017f5`, `8233d8d8`, `2623fa72`, `ed6c60f1` |
| 6 | **Calls** | Sibling calls, musttail perfect forwarding (stack args, varargs, indirect), the 4-register indirect hole, memory-form tail-call slot pinning, return-position libcall tail calls, conditional tail calls WITH the `TCRETURN_CC` opcode split (a conditional tail return sharing the unconditional descriptor is model-invalid -- `isBarrier`/`isReturn` on a maybe-taken exit -- so the split must land with the feature, not later), bitslice-starvation guard + RA-save elision | `3edb8a58`, `ccd3fe13`, `13a3e4cb`, `b884ab4d`, `30242f79`, `4cada17c`, `8d4c5674`, `9fcf6be8`, part of `f44c4db5` |
| 7 | **Codegen quality II** | Stack-address folding; zero-compare/`load_simmn`/global-offset folds; i64 carry chain, inline variable shifts, ordered compares via borrow; LSR post-increment preference with honest cost model; pool-load + immediate rematerialization (honest descriptors); SjLj receiver clobbers; select-of-constants fold; GlobalMerge at addPreISel; the clang `va_arg` over-alignment fix | `205380a0`, rest of `f44c4db5`, `e3b4c244`, `222f8f47`, `5dfec349`, `2341fef2`, `5c6aecb2`, `dadf3cd8`, `2a97553f`, `4cd99235`, `69fadd44`, `c7c36a66`, `cb42ac92` |
| 8 | **MachineOutliner** | Whitelist-closed classifier port | `0c0627ab` |
| 9 | **Predication & flags** | Sound conditional-branch modeling + `-verify-machineinstrs`; the GOTO/materializer `_CC` opcode splits with MIR coverage (`TCRETURN_CC` travels with series 6); condition flags modeled as the CFS physreg (fixes a live if-converter wrong-code bug); `ClobbersPredicate` SkipDead fix | `725f496a`, `b87fb163`, `f5f53f70`, `0b7d8aa8`, `9df5cf7f`, `e1b8b875` |
| 10 | **Machine model & scheduler** | Coarse SchedModel (the load-bearing omissions documented), the surviving `targetSchedulesPostRAScheduling()` override, post-RA scheduler wiring (−12% load-use stalls, size-neutral), per-def indexed-load latencies with the exact/hermetic tblgen-oracle test | `3aa5d899` (surviving override), `89b6cf64`, half of `42476f38`, `6f4481aa`, `168fcb84`, sched-doc part of `4bd729f4` |
| 11 | **tight_loop hardware loops** | EZHTightLoopFormation (n≥1 evidence rules, CFS discipline, budgets) with .ll + MIR evidence tests (incl. the de-vacuated `guard_taken_path` from `4bd729f4`); constant-island superset-invariant region protection (+ `-run-pass` registration, unresolved-Rend MIR test); the rotation-is-worthless verdict comment | `8a0c9102`, `cbe16be3`, rest of `42476f38`, `cd371f75`, evidence-test part of `4bd729f4`, `e7de9d49`, island/timing parts of `ad00de20`, `7be2cbf8`, `59e46c5c` |

Dependency notes: 10 requires 9, and the mechanism matters: post-RA
scheduling soundness comes from the CFS PHYSICAL-REGISTER dependencies
(S-forms define CFS; predicated and carry-consuming instructions use it) --
most predicated ALU/memory instances still share their opcodes, so distinct
`_CC` opcodes alone would not constrain the scheduler. The opcode splits
contribute descriptor/barrier/rematerialization honesty, not the scheduling
edges. 11 requires 9 and 10 (CFS-based evidence scans; bodies keep their
post-RA schedule). 1–8 are mutually independent of 9–11 and mostly of each
other; land 1–4 first since they are small and unblock everything
semantically.

## Not PR payload (stays on the fork, offered separately if wanted)

- `ezh/` demos and harness: two-board I2C/I3C ladder (M0–M5), event-fabric
  docs and probes, OpenOCD config, timing harness, portable runner, Docker
  handoff kit, `build_llvm.sh`.
- The 29 `ezh_lit_results.json` validation-artifact commits.
- **Dangling-pointer caution**: in-tree comments in `EZHSchedule.td`,
  `EZHTightLoopFormation.cpp`, and `IntrinsicsEZH.td` reference
  `ezh/TIGHT_LOOP_TIMING.md`. Series 10/11 must either carry that doc (e.g.
  as `llvm/lib/Target/EZH/EZHTimingNotes.md`) or reword the pointers.

## Flag to Richard directly (silicon facts his tree cannot derive)

1. `Inst{29}` (not `Inst{21}`) drives the 0x1D register-offset byte-load
   sign-extend; 0x1D address is `Rn + Rm` bytes. (Series 3.)
2. tight_loop is a FREE-RUNNING counted loop with a run-once slot after the
   instruction; Rcount+1 executions; registers latched at entry. The
   event-paced theory is disproven. (Series 4/11; AN14650 + cycle timing.)
3. Base vectored holds hardware-write RA (NVIC-style linkage). (Series 4.)
4. Load timing: 2-slot occupancy, data at issue+3, one filler free; stores
   drain 1-per-3-cycles; taken `goto_nz` = 3 cycles; APB reads ≈ 9 cycles.
   (Series 10's LoadLatency=2 rationale.)
5. His `fixupConditionalBr` conditional far-jump path still has no hardware
   coverage on either tree (KB-scale SRAM never exceeds the displacement).

## Mechanics

Prepare in a dedicated worktree stacking `series/1` … `series/11` as branches
off `origin/main`, each rebased-and-squashed from the source commits above.
Gate each tip per ground rule 2 (one board-day total for 11 silicon runs).
Open PRs in dependency order; keep `ezh-port` untouched as the evidence
branch that PR descriptions link into.
