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

# Vectored-hold priority measurement driver (one board). See ezh_vh_prio.c.
set -e
cd "$(dirname "$0")"
WT="$(cd ../.. && pwd)"
B=$WT/build/bin
: "${BOARD_SERIAL:?set BOARD_SERIAL to the probe serial}"
F="-target ezh-none-elf -mno-ezh-bitslice-interrupts -Os -ffreestanding -ffunction-sections -fdata-sections"
mkdir -p out_prio; cd out_prio
cat > minrt.c <<'EOF'
unsigned __mulsi3(unsigned a,unsigned b){unsigned r=0;while(b){if(b&1)r+=a;a<<=1;b>>=1;}return r;}
unsigned __udivsi3(unsigned a,unsigned b){unsigned q=0,r=0;for(int i=31;i>=0;i--){r=(r<<1)|((a>>i)&1);if(r>=b){r-=b;q|=1u<<i;}}return q;}
unsigned __umodsi3(unsigned a,unsigned b){unsigned r=0;for(int i=31;i>=0;i--){r=(r<<1)|((a>>i)&1);if(r>=b)r-=b;}return r;}
EOF
$B/clang $F -D__TEST__ -DSTACK_SIZE_WORDS=1280 -I $WT/ezh -I $WT/lldb/source/Plugins/Process/EZH $WT/ezh/crt0.c -c -o crt0.o
$B/clang $F minrt.c -c -o minrt.o
$B/clang $F -I .. -I ../.. ../ezh_vh_prio.c -c -o prio.o
$B/ld.lld -T ../../smartdma.ld --gc-sections --discard-locals crt0.o prio.o minrt.o -o prio.elf
echo "built out_prio/prio.elf"
cd ..

CFG=../rt595-openocd.cfg
if ! nc -z 127.0.0.1 4445 2>/dev/null; then
  EZH_ADAPTER_SERIAL="$BOARD_SERIAL" EZH_GDB_PORT=3334 EZH_TELNET_PORT=4445 \
    openocd -f "$CFG" >/tmp/ezh_ocd_prio.log 2>&1 &
  trap 'kill $! 2>/dev/null || true' EXIT
  sleep 3
fi

export EZH_READELF="$B/llvm-readelf"
python3 - <<'PY'
import os,sys,time,subprocess
sys.path.insert(0, os.getcwd())
from ezh_run_telnet import OCD
RE=os.environ["EZH_READELF"]
def syms(elf,names):
    d={}
    for L in subprocess.check_output([RE,"-s",elf]).decode().splitlines():
        f=L.split()
        if len(f)>=8 and f[7] in names: d[f[7]]=int(f[1],16)
    return d
S=syms(os.path.abspath("out_prio/prio.elf"),{"s_ready","s_go","s_state","s_v","exc_signal"})
b=OCD(4445); b.cmd("poll off")
def rd(x):
    for _ in range(6):
        v=b.mdw("0x%08x"%x)
        if v is not None: return v&0xffffffff
        b.cmd("poll off"); time.sleep(0.08)
    return -1
def w(x,v): b.cmd("mww 0x%08x 0x%08x"%(x,v))
def wait_ready(r,tmo=4.0):
    t0=time.time()
    while time.time()-t0<tmo:
        if rd(S["s_ready"])==r: return True
        time.sleep(0.05)
    return False
def db_fire(mask):                 # REQ toggled low first: genuine rising edge
    w(0x40027048, mask<<16)        # EN(mask), REQ low
    w(0x40027048, (mask<<16)|mask) # EN|REQ

# boot
b.cmd("halt")
w(0x40001040,0x40000000); w(0x40002634,0x00030000); w(0x4013500C,0x1)
w(0x40000040,0x40000000); w(0x40000070,0x40000000)
w(0x40027048,0x00000080)           # PENDTRAP hygiene
w(0x40027024,0xC0DE0000); b.cmd("load_image %s"%os.path.abspath("out_prio/prio.elf"),settle=0.7)
w(0x40027024,0xC0DE0010); w(0x40027020,0x24100000); w(0x40027024,0xC0DE0011)

plans=[("ch0 then ch1", [0x1,0x2]),
       ("ch1 then ch0", [0x2,0x1]),
       ("both in ONE write", [0x3]),
       ("both in ONE write (+queue test)", [0x3])]
print("=== which slice wins when both are pending at the hold? ===")
for r,(name,fires) in enumerate(plans, start=1):
    if not wait_ready(r):
        print("  round %d: core never became ready (state=0x%x)"%(r,rd(S["s_state"]))); break
    for m in fires:
        db_fire(m); time.sleep(0.05)
    w(S["s_go"], r)
    time.sleep(0.4)
    v=rd(S["s_v"]+(r-1)*8)
    print("  round %d (%-28s): winner = slice %d"%(r,name,v))
time.sleep(0.4)
st=rd(S["s_state"]); v2=rd(S["s_v"]+28); exc=rd(S["exc_signal"])
if st==42 or exc==0xCAFEBABE:
    if v2==0:
        print("  queue test: SECOND hold (no CFM rewrite) dispatched to slice 0 AGAIN ->")
        print("              a dispatch does NOT consume the sticky flags; every latched")
        print("              flag persists (winner keeps winning) until a full CFM write.")
    else:
        print("  queue test: SECOND hold dispatched to slice %d (the loser) -> the"%v2)
        print("              winner's flag was consumed by its dispatch.")
else:
    print("  queue test: second hold BLOCKED (state=0x%x) -> the dispatch consumed ALL"%st)
    print("              pending flags.")
print("  exc=0x%08x"%exc)
PY
