#!/usr/bin/env python3
# Copyright 2026 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Portable runner for the EZH *compiler* tests -- the half of the PR's
# validation that needs NO hardware, NO OpenOCD, and NO target runtimes: just a
# built clang/llc/llvm-mc/FileCheck. It executes the RUN: lines of every EZH
# MC/CodeGen/clang/Sema test directly (a tiny lit), so it works against any
# build tree on any OS, even one configured without the lit test infrastructure.
#
#   python3 ezh/check_ezh.py [path/to/build/bin]
#
# Exit code 0 iff every test passes. This does NOT run the on-silicon suites
# (ezh/*/run.sh) -- those require the EVK-MIMXRT595 board; see ezh/HANDOFF.md.

import os, re, sys, subprocess, tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Host-only EZH tests (relative to repo root). These are standard in-tree lit
# tests; we execute their RUN lines rather than relying on a configured lit.
TESTS = [
    "llvm/test/MC/EZH/instructions.s",
    "llvm/test/CodeGen/EZH/event-intrinsics.ll",
    "llvm/test/CodeGen/EZH/gpio-intrinsics.ll",
    "llvm/test/CodeGen/EZH/reg-offset.ll",
    "llvm/test/CodeGen/EZH/reg-shift.ll",
    "llvm/test/CodeGen/EZH/if-cvt-alu.ll",
    "llvm/test/CodeGen/EZH/indexed-memref.ll",
    "llvm/test/CodeGen/EZH/spill-memref.ll",
    "llvm/test/CodeGen/EZH/frame.ll",
    "clang/test/CodeGen/EZH/builtins-event.c",
    "clang/test/CodeGen/EZH/builtins-gpio.c",
    "clang/test/Sema/builtins-ezh.c",
]


def find_bin():
    if len(sys.argv) > 1:
        return os.path.abspath(sys.argv[1])
    for c in (os.path.join(REPO, "build", "bin"),):
        if os.path.isdir(c):
            return c
    sys.exit("error: pass the path to build/bin (could not autodetect)")


def run_lines(path, B, tmp):
    """Return (ok, first_failure_cmd, output). Executes each RUN: line; a file
    passes iff all of its RUN lines exit 0."""
    text = open(path).read()
    # Join RUN-line continuations (trailing backslash).
    joined, cont = [], ""
    for ln in text.splitlines():
        m = re.search(r"(?://|#|;)\s*RUN:\s*(.*)$", ln)
        if cont:
            cont += " " + ln.strip()
            if not ln.rstrip().endswith("\\"):
                joined.append(cont.rstrip("\\").strip()); cont = ""
            else:
                cont = cont.rstrip("\\").strip()
            continue
        if m:
            c = m.group(1)
            if c.rstrip().endswith("\\"):
                cont = c.rstrip("\\").strip()
            else:
                joined.append(c.strip())
    # Standard lit substitutions, minimal set these tests use.
    subs = [
        (r"%clang_cc1", f'"{B}/clang" -cc1'),
        (r"%clang\b", f'"{B}/clang"'),
        (r"%s", path),
        (r"%S", os.path.dirname(path)),
        (r"%t", tmp),
        (r"\bllvm-mc\b", f'"{B}/llvm-mc"'),
        (r"\bllc\b", f'"{B}/llc"'),
        (r"\bopt\b", f'"{B}/opt"'),
        (r"\bFileCheck\b", f'"{B}/FileCheck"'),
    ]
    for cmd in joined:
        for pat, rep in subs:
            cmd = re.sub(pat, rep, cmd)
        p = subprocess.run(["bash", "-c", cmd], cwd=REPO,
                           capture_output=True, text=True)
        if p.returncode != 0:
            return False, cmd, (p.stderr or p.stdout)
    return True, None, ""


def main():
    B = find_bin()
    for tool in ("clang", "llc", "llvm-mc", "FileCheck"):
        if not os.path.exists(os.path.join(B, tool)):
            sys.exit(f"error: {tool} not found in {B}")
    print(f"EZH compiler tests (host-only) -- binaries: {B}\n")
    npass = 0
    fails = []
    with tempfile.TemporaryDirectory() as td:
        for i, rel in enumerate(TESTS):
            path = os.path.join(REPO, rel)
            if not os.path.exists(path):
                print(f"  MISSING  {rel}"); fails.append(rel); continue
            ok, cmd, out = run_lines(path, B, os.path.join(td, "t%d" % i))
            print(f"  {'PASS' if ok else 'FAIL'}  {rel}")
            if ok:
                npass += 1
            else:
                fails.append(rel)
                print(f"        cmd: {cmd}")
                for l in out.strip().splitlines()[:6]:
                    print(f"        | {l}")
    print(f"\n{npass}/{len(TESTS)} passed")
    if fails:
        print("FAILED: " + ", ".join(fails)); sys.exit(1)
    print(">>> all EZH compiler tests pass (no hardware needed) <<<")


if __name__ == "__main__":
    main()
