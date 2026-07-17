#!/bin/bash
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

#===----------------------------------------------------------------------===#
# tight_loop body-shape cycle timing on silicon (see ezh_tightloop_timing.c).
#
# Clocks CTIMER0 from FRO_DIV1 and lets the EZH program measure its own
# tight_loop iterations against the free-running counter. Before running,
# statically verifies every emitted [tight_loop; nop; body...] sequence
# instruction by instruction -- a stray compiler-scheduled instruction inside
# the run-once slot or the repeated block would silently corrupt the
# measurement.
#===----------------------------------------------------------------------===#
set -e
cd "$(dirname "$0")"
WT="$(cd ../.. && pwd)"
B=$WT/build/bin
F="-target ezh-none-elf -O2 -mno-ezh-bitslice-interrupts -ffreestanding -ffunction-sections -fdata-sections"

mkdir -p out_tl_timing; cd out_tl_timing
$B/clang $F -D__TEST__ -DSTACK_SIZE_WORDS=1280 -I $WT/ezh -I $WT/lldb/source/Plugins/Process/EZH $WT/ezh/crt0.c -c -o crt0.o
$B/clang $F ../ezh_tightloop_timing.c -c -o timing.o
$B/ld.lld -T $WT/ezh/smartdma.ld --gc-sections --discard-locals crt0.o timing.o -o tl_timing.elf
echo "built out_tl_timing/tl_timing.elf"

$B/llvm-objdump --triple=ezh -d tl_timing.elf > tl_timing.dis
python3 - <<'PY'
# Static check: each tight_loop is followed by exactly [nop, <body>].
import re, sys
BODIES = [
    ["add_imm"],                            # 0 alu1
    ["add_imm", "add_imm"],                 # 1 alu2
    ["add_imm", "add_imm", "add_imm"],      # 2 alu3
    ["nop"],                                # 3 nop1
    # the 3-register add disassembles as its canonical add_lsl (shift 0) form
    ["ldr_post", "add_lsl"],                # 4 load_use
    ["add_lsl", "ldr_post"],                # 5 load_rot
    ["ldr_post", "add_imm", "add_lsl"],     # 6 load_space
    ["ldr_post"],                           # 7 load_only
    ["str_post"],                           # 8 store_only
    ["ldrb_post", "strb_post"],             # 9 copy
    ["strb_post", "ldrb_post"],             # 10 copy_rot
    ["ldr_post", "ldr_post"],               # 11 load2
    ["ldr"],                                # 15 perload_only (cases 12-14 are
                                            #    self-contained asm sw loops)
]
ins = []
for line in open("tl_timing.dis"):
    m = re.match(r"\s*[0-9a-f]+:\s+(?:[0-9a-f]{2}\s+)+\s*(\S+)", line)
    if m:
        ins.append(m.group(1))
locs = [i for i, m in enumerate(ins) if m == "tight_loop"]
if len(locs) != len(BODIES):
    sys.exit(f"FAIL: expected {len(BODIES)} tight_loops, found {len(locs)} "
             "(outer loop unrolled or cases merged?)")
for ci, i in enumerate(locs):
    want = ["nop"] + BODIES[ci]
    got = ins[i + 1 : i + 1 + len(want)]
    if got != want:
        sys.exit(f"FAIL case {ci}: after tight_loop expected {want}, got {got}")
print(f"static check OK: {len(locs)} tight_loops, every slot+body exact")
PY
cd ..

if ! nc -z 127.0.0.1 4444 2>/dev/null; then
  : "${BOARD_SERIAL:?openocd not running on 4444; set BOARD_SERIAL to start it}"
  EZH_ADAPTER_SERIAL="$BOARD_SERIAL" \
    openocd -f ../rt595-openocd.cfg >/tmp/ezh_ocd_tl_timing.log 2>&1 &
  trap 'kill $! 2>/dev/null || true' EXIT
  sleep 3
fi

export EZH_READELF="$B/llvm-readelf"
python3 - <<'PY'
import os, sys, time, subprocess
sys.path.insert(0, os.getcwd())
from ezh_run_telnet import OCD
ELF = os.path.abspath("out_tl_timing/tl_timing.elf")
RE = os.environ["EZH_READELF"]
syms = {}
for L in subprocess.check_output([RE, "-s", ELF]).decode().splitlines():
    f = L.split()
    if len(f) >= 8 and f[7] in ("exc_signal", "results", "valid"):
        syms[f[7]] = int(f[1], 16)
def rd(o, x):
    for _ in range(6):
        v = o.mdw("0x%08x" % x)
        if v is not None: return v & 0xffffffff
        o.cmd("poll off"); time.sleep(0.08)
    return -1
o = OCD(4444); o.cmd("poll off"); o.cmd("halt")
for a in ["mww 0x40001040 0x40000000","mww 0x40002634 0x00030000","mww 0x4013500C 0x1",
          "mww 0x40000040 0x40000000","mww 0x40000070 0x40000000"]:
    o.cmd(a)
# CTIMER0: clock from FRO_DIV1, release reset, prescaler 0, enable.
for a in ["mww 0x40021048 0x1","mww 0x400217A0 0x1","mww 0x40020078 0x1",
          "mww 0x40028004 0x2","mww 0x4002800C 0x0","mww 0x40028004 0x1"]:
    o.cmd(a)
tc0 = rd(o, 0x40028008); time.sleep(0.1); tc1 = rd(o, 0x40028008)
if tc1 == tc0:
    print("FAIL: CTIMER0 TC not counting (tc=0x%08X)" % tc0); sys.exit(1)
print("CTIMER0 counting: ~%.1f MHz" % ((tc1 - tc0) / 0.1 / 1e6))
o.cmd("mww 0x40027024 0xC0DE0000")
o.cmd("load_image %s" % ELF, settle=0.7)
o.cmd("mww 0x40027024 0xC0DE0010"); o.cmd("mww 0x40027020 0x24100000"); o.cmd("mww 0x40027024 0xC0DE0011")
time.sleep(1.5)
exc = rd(o, syms["exc_signal"])
print("exc_signal = 0x%08X" % exc)
if exc != 0xCAFEBABE:
    print("FAIL: program did not complete cleanly"); sys.exit(1)
NAMES = ["alu1", "alu2", "alu3", "nop1", "load_use", "load_rot", "load_space",
         "load_only", "store_only", "copy", "copy_rot", "load2",
         "fill_sw", "sum_sw", "copy_sw", "perload"]
r = [rd(o, syms["results"] + 4 * i) for i in range(len(NAMES))]
vv = [rd(o, syms["valid"] + 4 * i) for i in range(len(NAMES))]
bad = [(n, m) for n, m in zip(NAMES, vv) if m != 1]
for n, m in bad:
    print("INVALID: case %s failed its architectural-state check (0x%08X)" % (n, m))
ITERS = 50 * 2000
q = (r[1] - r[0]) / ITERS  # ticks per core cycle
print("tick quantum q = %.4f ticks/cycle  (alu2-alu1 over %d iters)" % (q, ITERS))
print("%-11s %10s %12s" % ("case", "ticks", "cycles/iter"))
for n, v, m in zip(NAMES, r, vv):
    print("%-11s %10d %12.3f%s" % (n, v, v / (q * ITERS), "" if m == 1 else "  << INVALID"))
w = r[0] / (q * ITERS) - 1
print("\nderived: wrap cost w = %.3f cycles" % w)
print("         s_in   (dist-1 load-use in block)  = %.3f" % (r[4] / (q * ITERS) - 2 - w))
print("         s_wrap (dist-1 load-use over wrap) = %.3f" % (r[5] / (q * ITERS) - 2 - w))
print("         s_dist2                            = %.3f" % (r[6] / (q * ITERS) - 3 - w))
print("\n>>> TIGHT_LOOP TIMING RUN %s <<<" % ("COMPLETE" if not bad else "HAS INVALID CASES"))
sys.exit(0 if not bad else 1)
PY
