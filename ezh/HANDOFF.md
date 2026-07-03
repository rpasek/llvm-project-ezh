<!--
Copyright 2026 Google LLC
Licensed under the Apache License, Version 2.0 (the "License").
-->

# EZH backend — handoff / how to run the tests elsewhere

The tests split into **two tiers**, and only one needs the board. This is the
whole reason a handoff looks hard but isn't: the compiler work is validated by
ordinary in-tree LLVM lit tests that run on any machine; the runtime behaviour
is validated on real silicon and needs the hardware.

| Tier | What it validates | Needs the board? | How a colleague runs it |
|------|-------------------|:----------------:|-------------------------|
| **1. Compiler tests** | MC encodings (diffed against the silicon-derived ISA), ISel patterns, clang builtins, Sema immediate ranges | **No** | Docker one-liner, or a normal LLVM build + one script |
| **2. On-silicon suites** | Runtime correctness of generated code on the EZH core | **Yes** (EVK‑MIMXRT595 + LPC‑LINK2) | Remote board, shipped logs, or shipped board |

Tier 1 is the bulk of a compiler review and is 100% portable. Tier 2 is the
hardware proof; a colleague without the board reviews its logs or runs it
against a shared board.

---

## Tier 1 — compiler tests (any OS, no hardware)

### Option A — Docker (zero local setup; recommended for a fresh machine)

```sh
docker build -t ezh-tests -f ezh/Dockerfile ezh
```

Clones the PR branch, builds `clang`/`llc`/`llvm-mc`/`FileCheck`, and runs the
12 EZH compiler tests as the final layer. **The build succeeds iff every test
passes.** No macOS quirks, no OpenOCD, no target runtimes — just Docker.

### Option B — against an existing / local LLVM build

Any build of this branch that includes the `EZH` target and `clang` works —
Linux or macOS. From the repo root:

```sh
python3 ezh/check_ezh.py <path/to/build/bin>
```

`check_ezh.py` is a tiny self-contained lit: it executes the `RUN:` lines of
every EZH MC/CodeGen/clang/Sema test directly, so it works even against a
build configured *without* the lit test infrastructure. Exit 0 iff all pass.

If the build *was* configured with tests (`-DLLVM_INCLUDE_TESTS=ON`, the
default), the idiomatic route also works:

```sh
ninja -C build check-llvm-codegen-ezh check-llvm-mc check-clang
```

### What Tier 1 covers

- `llvm/test/MC/EZH/instructions.s` — the golden encoding test, generated from
  every macro in NXP's `fsl_smartdma_prv.h` and cross-checked against the
  silicon-derived `ezhdis` disassembler. Runs both directions (assemble→encode,
  encode→disassemble).
- `llvm/test/CodeGen/EZH/*.ll` — ISel: the intrinsics, reg-offset/shift folds,
  if-conversion, indexed/spill MMOs, frame lowering.
- `clang/test/CodeGen/EZH/builtins-*.c` — every `__builtin_ezh_*` lowers to the
  right intrinsic.
- `clang/test/Sema/builtins-ezh.c` — out-of-range immediate constants are
  diagnosed at the call site.

---

## Tier 2 — on-silicon suites (needs the EVK‑MIMXRT595)

These run the compiled programs on the real EZH core over JTAG (OpenOCD gdb
server on `:3333`, driven by the custom `lldb` `ezh-remote` plugin):

- `ezh/ezh_test/run.sh` — 16 basic samples.
- `ezh/llvm_test/run.sh` — the full SingleSource regression suite at `-O0`
  and `-Os` (**3078 tests**, ~8 min).

Prereqs (documented in `ezh/BUILDING.md`): the custom `lldb`, the EZH runtimes
(`build/libc`, `libc_nano`, `compiler-rt` builtins — build with `build_llvm.sh`,
which handles the macOS `ar`/`ranlib` and compiler-rt `SYSTEM_NAME` pitfalls;
on Linux those pitfalls mostly vanish), the `llvm-test-suite` checkout, and a
running OpenOCD:

```sh
EZH_ADAPTER_SERIAL=<probe-serial> openocd -f ezh/rt595_openocd.cfg   # gdb :3333
```

> Note: `ezh/rt595_openocd.cfg` is tracked on the `ezh-test` branch but not on
> the PR branch, so a checkout of the PR branch removes it from disk. Restore
> with `git show ezh-test:ezh/rt595_openocd.cfg > ezh/rt595_openocd.cfg`.

### Three ways to give a colleague Tier 2

1. **Ship the logs (no hardware for them).** `ezh/ezh_test/ezh_regression_summary.txt`
   plus the captured `run.sh` output are the evidence of the pass. Best for a
   reviewer who just needs to see it is green. Latest: **3078/3078 (100%)**,
   reproduced twice on an EVK‑MIMXRT595.

2. **Remote board (they run the real suite against your hardware).** Keep the
   board on a machine you control; expose the OpenOCD gdb server over SSH:

   ```sh
   # on the board host:
   EZH_ADAPTER_SERIAL=<serial> openocd -f ezh/rt595_openocd.cfg    # listens on :3333
   # on the colleague's machine (forward :3333 to your board host):
   ssh -N -L 3333:localhost:3333 you@board-host
   ```

   Their locally-built `lldb` + `ezh_lit_runner` connects to `localhost:3333`
   as if the board were local. This gives real silicon results without shipping
   hardware. (The runner already targets `connect://localhost:3333`.)

3. **Ship the board.** The EVK‑MIMXRT595 + LPC‑LINK2 probe, plus this repo; they
   follow `ezh/BUILDING.md`. Highest fidelity, highest logistics.

---

## Recommendation

For a **compiler-PR review**: Tier 1 (Docker, one command) covers the entire
contribution; attach the Tier 2 logs as the hardware proof. Only stand up a
board (remote or shipped) if the reviewer specifically needs to re-validate
runtime behaviour rather than the compiler.

Pinned base: the stack sits on `origin/main` of Richard Pasek's tree; the
branch is `ezh-port`. The MC golden test is the encoding oracle — if it passes,
the assembler/disassembler match the silicon-derived encodings byte-for-byte.
