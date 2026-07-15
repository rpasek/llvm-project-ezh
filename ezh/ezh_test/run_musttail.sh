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
# Tail-call / musttail on-silicon self-test (single board).
#
# Runs musttail_selftest.c (+ musttail_fwd.ll for the vararg shape) on the
# EZH and checks six runtime results plus the clean-exit signal. Needs one
# EVK-MIMXRT595 with openocd already serving telnet on 4444 (or set
# BOARD_SERIAL to have this script start it).
#===----------------------------------------------------------------------===#
set -e
cd "$(dirname "$0")"
WT="$(cd ../.. && pwd)"
B=$WT/build/bin
F="-target ezh-none-elf -Os -ffreestanding -ffunction-sections -fdata-sections"
mkdir -p out_musttail; cd out_musttail

cat > minrt.c <<'EOF'
unsigned __mulsi3(unsigned a,unsigned b){unsigned r=0;while(b){if(b&1)r+=a;a<<=1;b>>=1;}return r;}
int __modsi3(int a,int b){int r=a;while(r>=b)r-=b;return r;}
EOF
$B/clang $F -D__TEST__ -DSTACK_SIZE_WORDS=1280 -I $WT/ezh -I $WT/lldb/source/Plugins/Process/EZH $WT/ezh/crt0.c -c -o crt0.o
$B/clang $F ../musttail_selftest.c -c -o selftest.o
$B/clang $F ../musttail_fwd.ll -c -o fwd.o 2>/dev/null
$B/clang $F minrt.c -c -o minrt.o
$B/ld.lld -T $WT/ezh/smartdma.ld --gc-sections --discard-locals crt0.o selftest.o fwd.o minrt.o -o musttail_selftest.elf
echo "built out_musttail/musttail_selftest.elf"
cd ..

if ! nc -z 127.0.0.1 4444 2>/dev/null; then
  : "${BOARD_SERIAL:?openocd not running on 4444; set BOARD_SERIAL to start it}"
  EZH_ADAPTER_SERIAL="$BOARD_SERIAL" \
    openocd -f ../rt595-openocd.cfg >/tmp/ezh_ocd_musttail.log 2>&1 &
  trap 'kill $! 2>/dev/null || true' EXIT
  sleep 3
fi

export EZH_READELF="$B/llvm-readelf"
python3 - <<'PY'
import os, sys, time, subprocess
sys.path.insert(0, os.getcwd())
from ezh_run_telnet import OCD
ELF = os.path.abspath("out_musttail/musttail_selftest.elf")
RE = os.environ["EZH_READELF"]
syms = {}
for L in subprocess.check_output([RE, "-s", ELF]).decode().splitlines():
    f = L.split()
    if len(f) >= 8 and f[7] in ("exc_signal","r1","r2","r3","r4","r5","r6"):
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
vals = {k: rd(o, syms[k]) for k in ("r1","r2","r3","r4","r5","r6")}
exp = {"r1":60,"r2":90,"r3":60,"r4":24,"r5":116,"r6":30}
print("exc_signal = 0x%08X" % exc)
for k in ("r1","r2","r3","r4","r5","r6"):
    print("  %s = %d (expect %d)" % (k, vals[k], exp[k]))
ok = exc == 0xCAFEBABE and all(vals[k] == exp[k] for k in exp)
print("\n>>> MUSTTAIL SELF-TEST %s <<<" % ("PASSED" if ok else "FAILED"))
sys.exit(0 if ok else 1)
PY
