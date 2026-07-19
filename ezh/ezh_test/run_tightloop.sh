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
# Silicon self-test for compiler-formed tight_loop hardware loops.
#
# Builds tightloop_selftest.c at -O2 with bitslice interrupts disabled (the
# configuration in which EZHTightLoopFormation fires), statically verifies
# that the hardware loops were actually formed (and that the shared-counter
# shape was NOT converted), then executes on the board over the OpenOCD
# telnet port and checks exc_signal and the sum results.
#===----------------------------------------------------------------------===#
set -e
cd "$(dirname "$0")"
WT="$(cd ../.. && pwd)"
B=$WT/build/bin
F="-target ezh-none-elf -O2 -mno-ezh-bitslice-interrupts -ffreestanding -ffunction-sections -fdata-sections"

mkdir -p out_tightloop; cd out_tightloop
$B/clang $F -D__TEST__ -DSTACK_SIZE_WORDS=1280 -I $WT/ezh -I $WT/lldb/source/Plugins/Process/EZH $WT/ezh/crt0.c -c -o crt0.o
$B/clang $F ../tightloop_selftest.c -c -o selftest.o
$B/ld.lld -T $WT/ezh/smartdma.ld --gc-sections --discard-locals crt0.o selftest.o -o tightloop_selftest.elf
echo "built out_tightloop/tightloop_selftest.elf"

# Static verification: the three pump loops formed hardware loops; the
# shared-counter countdown kept its ordinary backedge.
NTL=$($B/llvm-objdump --triple=ezh -d tightloop_selftest.elf | grep -c "tight_loop" || true)
if [ "$NTL" -lt 3 ]; then
  echo "FAIL: expected >=3 compiler-formed tight_loops, found $NTL"
  exit 1
fi
CD=$($B/llvm-objdump --triple=ezh -d tightloop_selftest.elf | awk '/<countdown>:/{f=1} f&&/goto_nz/{print; exit}')
if [ -z "$CD" ]; then
  echo "FAIL: countdown (shared-counter) lost its ordinary backedge"
  exit 1
fi
echo "static check OK: $NTL tight_loops formed; countdown kept goto_nz"
cd ..

if ! nc -z 127.0.0.1 4444 2>/dev/null; then
  : "${BOARD_SERIAL:?openocd not running on 4444; set BOARD_SERIAL to start it}"
  EZH_ADAPTER_SERIAL="$BOARD_SERIAL" \
    openocd -f ../rt595-openocd.cfg >/tmp/ezh_ocd_tightloop.log 2>&1 &
  trap 'kill $! 2>/dev/null || true' EXIT
  sleep 3
fi

export EZH_READELF="$B/llvm-readelf"
python3 - <<'PY'
import os, sys, time, subprocess
sys.path.insert(0, os.getcwd())
from ezh_run_telnet import OCD
ELF = os.path.abspath("out_tightloop/tightloop_selftest.elf")
RE = os.environ["EZH_READELF"]
syms = {}
for L in subprocess.check_output([RE, "-s", ELF]).decode().splitlines():
    f = L.split()
    if len(f) >= 8 and f[7] in ("exc_signal", "g_sum1", "g_sum7", "g_sum64"):
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
o.cmd("mww 0x40027024 0xC0DE0000")
o.cmd("load_image %s" % ELF, settle=0.7)
o.cmd("mww 0x40027024 0xC0DE0010"); o.cmd("mww 0x40027020 0x24100000"); o.cmd("mww 0x40027024 0xC0DE0011")
time.sleep(1.5)
exc = rd(o, syms["exc_signal"])
s1, s7, s64 = rd(o, syms["g_sum1"]), rd(o, syms["g_sum7"]), rd(o, syms["g_sum64"])
print("exc_signal = 0x%08X" % exc)
print("  sum(n=1) = %d (expect 7)" % s1)
print("  sum(n=7) = %d (expect 140)" % s7)
print("  sum(n=64) = %d (expect 85792)" % s64)
ok = exc == 0xCAFEBABE and s1 == 7 and s7 == 140 and s64 == 85792
print("\n>>> TIGHT_LOOP CODEGEN SELF-TEST %s <<<" % ("PASSED" if ok else "FAILED"))
sys.exit(0 if ok else 1)
PY
