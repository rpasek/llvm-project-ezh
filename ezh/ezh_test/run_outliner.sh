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
# MachineOutliner differential self-test (single board).
#
# Builds outliner_selftest.c TWICE at -Oz -- outliner ON (default) and OFF
# (-mllvm -enable-machine-outliner=never) -- confirms outlining actually
# fired in the ON build (OUTLINED_FUNCTION_* symbols) and that its outlined
# bodies contain only whitelisted ops, then runs BOTH on the EVK-MIMXRT595
# and asserts the result tables are byte-identical. A whitelist miscompile
# makes the ON table diverge.
#===----------------------------------------------------------------------===#
set -e
cd "$(dirname "$0")"
WT="$(cd ../.. && pwd)"
B=$WT/build/bin
F="-target ezh-none-elf -Oz -ffreestanding -ffunction-sections -fdata-sections"
mkdir -p out_outliner; cd out_outliner

build() { # $1 = tag, $2... = extra flags
  local tag=$1; shift
  $B/clang $F "$@" -D__TEST__ -DSTACK_SIZE_WORDS=1280 -I $WT/ezh -I $WT/lldb/source/Plugins/Process/EZH $WT/ezh/crt0.c -c -o crt0_$tag.o
  $B/clang $F "$@" ../outliner_selftest.c -c -o self_$tag.o
  $B/ld.lld -T $WT/ezh/smartdma.ld --gc-sections --discard-locals crt0_$tag.o self_$tag.o -o self_$tag.elf
}
build on
build off -mllvm -enable-machine-outliner=never

NOUT=$($B/llvm-objdump --triple=ezh -d self_on.elf | grep -c "OUTLINED_FUNCTION" || true)
if [ "$NOUT" -eq 0 ]; then echo "FAIL: outliner did not fire in the ON build"; exit 1; fi
# Every outlined body: only whitelisted opcodes + one trailing return; no
# branch/call/_s/predicated/adc/sbc/ra-write/pc-relative-load.
BAD=$($B/llvm-objdump --triple=ezh -d self_on.elf \
  | awk '/<OUTLINED_FUNCTION_[0-9]+>:/{o=1;next} o&&/^[0-9a-f]+ <[A-Za-z_]/{o=0} o&&/mov[ \t]+pc, ra/{o=0;next} o{print}' \
  | grep -viE "add |sub |and |or |xor |mov |lsl|lsr|asr|ror|andor|load_imm|load_simm|gotol_bs" || true)
if [ -n "$BAD" ]; then echo "FAIL: non-whitelisted op in an outlined body:"; echo "$BAD"; exit 1; fi
echo "outliner fired: $NOUT outlined functions, bodies clean"
cd ..

if ! nc -z 127.0.0.1 4444 2>/dev/null; then
  : "${BOARD_SERIAL:?openocd not running on 4444; set BOARD_SERIAL to start it}"
  EZH_ADAPTER_SERIAL="$BOARD_SERIAL" openocd -f ../rt595-openocd.cfg >/tmp/ezh_ocd_outl.log 2>&1 &
  trap 'kill $! 2>/dev/null || true' EXIT
  sleep 3
fi

export EZH_READELF="$B/llvm-readelf"
python3 - <<'PY'
import os, sys, time, subprocess
sys.path.insert(0, os.getcwd())
from ezh_run_telnet import OCD
RE = os.environ["EZH_READELF"]
def syms(elf):
    d = {}
    for L in subprocess.check_output([RE, "-s", elf]).decode().splitlines():
        f = L.split()
        if len(f) >= 8 and f[7] in ("exc_signal", "g_results"):
            d[f[7]] = int(f[1], 16)
    return d
def rd(o, x):
    for _ in range(6):
        v = o.mdw("0x%08x" % x)
        if v is not None: return v & 0xffffffff
        o.cmd("poll off"); time.sleep(0.08)
    return -1
def run(elf):
    S = syms(elf)
    o = OCD(4444); o.cmd("poll off"); o.cmd("halt")
    for a in ["mww 0x40001040 0x40000000","mww 0x40002634 0x00030000","mww 0x4013500C 0x1",
              "mww 0x40000040 0x40000000","mww 0x40000070 0x40000000"]:
        o.cmd(a)
    o.cmd("mww 0x40027024 0xC0DE0000")
    o.cmd("load_image %s" % os.path.abspath(elf), settle=0.7)
    o.cmd("mww 0x40027024 0xC0DE0010"); o.cmd("mww 0x40027020 0x24100000"); o.cmd("mww 0x40027024 0xC0DE0011")
    time.sleep(1.2)
    exc = rd(o, S["exc_signal"])
    res = [rd(o, S["g_results"] + i*4) for i in range(26)]
    del o
    return exc, res

exc_on, res_on = run("out_outliner/self_on.elf")
time.sleep(0.3)
exc_off, res_off = run("out_outliner/self_off.elf")
print("ON  exc=0x%08X" % exc_on)
print("OFF exc=0x%08X" % exc_off)
ok = (exc_on == 0xCAFEBABE and exc_off == 0xCAFEBABE and res_on == res_off)
if res_on != res_off:
    for i,(a,b) in enumerate(zip(res_on,res_off)):
        if a != b: print("  MISMATCH [%d] on=0x%08x off=0x%08x" % (i,a,b))
print("\n>>> OUTLINER DIFFERENTIAL %s (%d results identical) <<<" %
      ("PASSED" if ok else "FAILED", sum(1 for a,b in zip(res_on,res_off) if a==b)))
sys.exit(0 if ok else 1)
PY
