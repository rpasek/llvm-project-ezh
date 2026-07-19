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

# EZH port plan — our `ezh-test` work → Richard's reorganized `origin/main`

> **STATUS (2026-07-18): COMPLETE — historical record.** All phases landed on
> `ezh-port` (110 commits over `cf402fe`), silicon-validated throughout
> (final baseline: 4,617/4,617 across three suite configs). One Phase-0
> decision was ultimately reversed: we adopted HIS bare mnemonics rather
> than adding the `e_` prefix — the corpus was retooled instead. The next
> step is the upstream PR series: see `PORT_UPSTREAM.md` (v2).

## Situation

Richard Pasek (`rpasek@google.com`) force-pushed `origin/main` and deleted the
remote `ezh-test`. His `main` is a clean ~12-commit **upstreaming-style** EZH
stack (copy Lanai → copy ARM constant-island → rename → add backend →
clang/compiler-rt/libc/libunwind/lld/lldb → tests), rebased onto upstream LLVM
**~2042 commits newer** than our 2026-06-02 merge-base. It contains **none** of
our improvements.

Conventions differ: **no `e_` mnemonic prefix** (`ldr_reg`), **`GPR`** (not
`GPRAll`), **`pred:$Cond`** predication operand (not our `#cc.Suffix` foreach),
named opcode constants, **`EM_EZH = 14650`** (not our `0x6BA0`).

**Verified directly against `origin/main`:** his base *reproduces our
correctness bugs* — the 3 MC silent-miscompiles (`assert((val&3)==0)`×3, no
`getImm5OpValue`, `FIXUP_EZH_32` default in the expr branch), the 0x1D reg-offset
sign-extend bug (`EZHInstMemReg` drives `signedAccess` at bit 21, no `Inst{29}`),
and the missing MMOs (`setNodeMemRefs` absent). Meanwhile several of our items are
**moot** — his base independently has generalized constant materialization, a
correct reg-shift encoding (with the N-form patterns we deferred), a real
conditional far-branch, predication-for-free via `pred:$Cond`, and a clean
`LowerCallResult`.

## Convention decisions (settle these first — Phase 0)

| Convention | Decision | Why |
|---|---|---|
| `e_` mnemonic prefix | **Adopt ours** — add `e_` to his ~200 AsmStrings + the pushd/popd InstPrinter constants; keep his `pred:$Cond` | AN14650 documents the ISA as `E_LDR`/`E_GOTO`/… — the `e_` prefix *is* the real ISA. Prefixing one backend leaves our 339-file `ezh/` corpus + 13 in-tree tests + all silicon-validated inline asm untouched; stripping `e_` from our side churns 350+ files. |
| `GPR` vs `GPRAll` | **Adopt his `GPR`**; rename `GPRAll→GPR`, `tcGPR→GPRNoFPBP`, `GPRLow→GPR3Bit` | They are the identical 16-register set (`GPRAll` is a pure alias). His `GPR` already includes the special regs (`GPO/GPD/CFS/CFM/GPI/…`); the rename is loss-free. Inline asm uses register *names*, not class names. |
| Predication model | **Adopt his `pred:$Cond`** wholesale; drop our foreach-cc expansion + the 3 C++ allow-list switches | Operand-based predication is the ARM/AArch64/Hexagon idiom reviewers expect; ours bloats the opcode table ~16×. Our two predication-widening commits are **moot** — his bit-ops/ADC/SBC/PER are already predicable for free. |
| `EM_EZH` value | **Adopt his `14650`**; port only the Object-layer *wiring* he lacks (ELFObjectFile/ELF.cpp/ELFYAML/ELFDumper — all symbolic, inherit 14650) | Neither value is gABI-registered (no correctness criterion); the tree we port *onto* should win to minimize divergence. Keeping `0x6BA0` is a gratuitous permanent header diff. |

## Sequenced phases

- **Phase 0 — Settle conventions** (above). Global find/replace-class decisions; blocking for any `.td`/instruction-def C++.
- **Phase 1 — Correctness fixes his base also has** (highest value, convention-light):
  1. **mc-diagnostics** — symbol-imm `FIXUP_EZH_32` clobber, NDEBUG-erased misaligned-branch asserts, imm5 truncation → diagnose. *(all 3 verified present)*
  2. **reg-offset ENCODING fix** — `signedAccess` `Inst{21}→Inst{29}` in `EZHInstMemReg`. *(bit-21 bug verified present; silicon-validated; flag to Richard with AN14650/EVK evidence)*
  3. **MMOs** — `setNodeMemRefs` in DAGToDAG + `mayLoad/mayStore` on indexed defs + spill/reload `addMemOperand`. *(all 3 sites verified buggy)*
- **Phase 2 — Codegen-quality folds that are real gaps**: reg-offset add-fold patterns (`a[i]`); signed-compare constant-flip + `EZHbtst`/`PseudoBTSTi`; SchedRW wiring + dead-itinerary trim; `EZHBranchFixup` wiring **with a new unconditional-only guard** (his `GOTO` is one predicated def — porting naively would drop a taken edge).
- **Phase 3 — Feature layer** (large; critical path): re-author `IntrinsicsEZH.td` (16 intrinsics) + `BuiltinsEZH` (prefer `.td` on his base) + the 15 intrinsic→instruction bindings in his idiom → then **sema-ranges** (as `SemaEZH.cpp::CheckEZHBuiltinFunctionCall`, since per-target checks moved out of `SemaChecking.cpp`) → then `ezh.h` (drop `e_`… no, keep `e_`) → then the `ezh/` corpus (I2C + event-fabric examples + docs). **~8 corpus files call `__builtin_ezh_*` and will not compile until the builtin layer lands.**
- **Phase 4 — Test/tooling**: ezhdis differential harness as-is (mnemonic-agnostic); test residue of the moot items (`constant-materialization.ll`, if-cvt bitops coverage, reg-shift negative MC test).

## Moot / superseded — do NOT port the code (carry only test residue + silicon notes)

- **condbranch-guard** — his `ConstantIslandPass` implements a *real* conditional far-branch (`LOAD_CONSTANT_COND`); our `report_fatal_error` would **regress** it.
- **regshift-encoding** — his encoding is already correct *and* has the N-form ISel patterns we deferred.
- **isel-constmat C++** — his `LowerConstant` is already generalized.
- **predication-widen C++** — moot (his `pred:$Cond`).
- **errs() dump cleanup** — his `LowerCallResult` is already clean.

## Biggest risks

1. **The reg-offset `Inst{29}` fix is a silicon fact his Lanai/ARM-derived line cannot derive.** If not flagged to Richard with AN14650/EVK evidence, his `LDR_REGBS` ships zero-extending the moment the fold patterns land (Phase 2). Encoding fix MUST precede/accompany the pattern half.
2. **`signbit-bittest` `PseudoBTSTi` custom inserter** must append his always-true `pred:$Cond` operand our version omits — wrong operand order silently mis-predicates. Mirror his `PseudoCMP`/`PseudoBR_CC` inserters exactly. Highest-risk translation.
3. **`EZHBranchFixup` on his single predicated `GOTO`** — needs an explicit unconditional-only guard or it drops a taken conditional edge. Re-validate; our silicon test was unconditional-only.
4. **imm5 ImmLeaf tightening** (`isInt<5>||isUInt<5>`→`isUInt<5>`) could regress one of his patterns — full backend rebuild + lit his MC/CodeGen tests before landing.
5. **Feature cascade**: sema-ranges + ~8 corpus files are hard-blocked on the large intrinsics-builtins re-author. Sequence builtins first; it's the critical path.
6. **`SemaChecking` refactor on the newer base** — per-target checks moved to `SemaXXX.cpp`, builtin DBs to `.td`. Re-author as `SemaEZH.cpp` + `BuiltinsEZH.td`, not a verbatim `case ezh:`.

## Recommended first PR (proof of concept)

**mc-diagnostics** — the three "diagnose instead of silently mis-assemble" fixes
in `EZHMCCodeEmitter.cpp` + the imm5 td tightening + three `llvm-mc` negative
tests. All three bugs verified present in `origin/main`; the C++ APIs
(`reportError`, `MCFixup::create`) are already used in his `getPerAddrOpValue`
(zero API adaptation); it's convention-light (only the `e_→bare` mnemonic
translation in error hints/tests + his fixup-enum names `FIXUP_EZH_HI16→…_11`,
`LO16→…_12`). It exercises exactly the `e_`-prefix + fixup-naming muscles every
later PR needs, with no predication/feature dependency. Gate on a full backend
rebuild + lit of his existing EZH tests (clears the imm5 risk).

## Flag to Richard directly (silicon facts his upstreaming line can't derive)

- 0x1D reg-offset byte-load sign-extend is **`Inst{29}`, not `Inst{21}`** (EVK-MIMXRT595-validated; his base has the bit-21 bug).
- 0x1D reg-offset address = `Rn + Rm` (byte offset), validated.
- His `fixupConditionalBr`'s `LOAD_CONSTANT_COND` conditional far-jump has **no hardware coverage** on either tree (KB-scale SRAM never exceeds the 8MB displacement) — a validation task on his base, not a port.
