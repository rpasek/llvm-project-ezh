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
# PREEMPTIVE-MODEL demo: tiling as mainline code, I3C as a real ISR.
#
#   board B (slave): i3c_preempt_slave.c compiled WITH bitslice interrupts
#     (no -mno-ezh-bitslice-interrupts): the compiler injects conditional
#     calls to crt0's bitslice_handler at every branch, so the tile-copy
#     mainline is preempted into vector0() by each I3C0 IRQ.
#   board A (master): i3c_sdr_master.c (unchanged M3 master, hold-based) --
#     RSTDAA, ENTDAA assigning dynamic address 0x30, 17-byte SDR write.
#
# Same J18 wiring as the other demos. This is the counterpart of
# run_i3c_vectored.sh: same bus traffic, opposite interrupt model.
#===----------------------------------------------------------------------===#
set -e
cd "$(dirname "$0")"
WT="$(cd ../.. && pwd)"
B=$WT/build/bin
: "${BOARD_A_SERIAL:?set BOARD_A_SERIAL to the master probe serial}"
: "${BOARD_B_SERIAL:?set BOARD_B_SERIAL to the slave probe serial}"
# Slave: bitslice interrupts ON (no -mno flag). Master: hold-based as before.
FS="-target ezh-none-elf -Os -ffreestanding -ffunction-sections -fdata-sections"
FM="-target ezh-none-elf -mno-ezh-bitslice-interrupts -Os -ffreestanding -ffunction-sections -fdata-sections"
mkdir -p out_i3c_pre; cd out_i3c_pre

cat > minrt.c <<'EOF'
unsigned __mulsi3(unsigned a,unsigned b){unsigned r=0;while(b){if(b&1)r+=a;a<<=1;b>>=1;}return r;}
unsigned __udivsi3(unsigned a,unsigned b){unsigned q=0,r=0;for(int i=31;i>=0;i--){r=(r<<1)|((a>>i)&1);if(r>=b){r-=b;q|=1u<<i;}}return q;}
unsigned __umodsi3(unsigned a,unsigned b){unsigned r=0;for(int i=31;i>=0;i--){r=(r<<1)|((a>>i)&1);if(r>=b)r-=b;}return r;}
EOF
# slave stack: everything with injection ON
$B/clang $FS -D__TEST__ -DSTACK_SIZE_WORDS=1280 -I $WT/ezh -I $WT/lldb/source/Plugins/Process/EZH $WT/ezh/crt0.c -c -o crt0_bs.o
$B/clang $FS minrt.c -c -o minrt_bs.o
$B/clang $FS -I .. -I ../.. ../i3c_preempt_slave.c -c -o slave.o
$B/ld.lld -T ../../smartdma.ld --gc-sections --discard-locals crt0_bs.o slave.o minrt_bs.o -o i3c_preempt_slave.elf
# master stack: hold-based as in the other demos
$B/clang $FM -D__TEST__ -DSTACK_SIZE_WORDS=1280 -I $WT/ezh -I $WT/lldb/source/Plugins/Process/EZH $WT/ezh/crt0.c -c -o crt0.o
$B/clang $FM minrt.c -c -o minrt.o
$B/clang $FM -I .. -I ../.. ../i3c_sdr_master.c -c -o master.o
$B/ld.lld -T ../../smartdma.ld --gc-sections --discard-locals crt0.o master.o minrt.o -o i3c_sdr_master.elf
echo "built out_i3c_pre/{i3c_preempt_slave,i3c_sdr_master}.elf"
cd ..

CFG=../rt595-openocd.cfg
PIDS=()
if ! nc -z 127.0.0.1 4444 2>/dev/null; then
  EZH_ADAPTER_SERIAL="$BOARD_A_SERIAL" \
    openocd -f "$CFG" >/tmp/ezh_ocd_boardA.log 2>&1 &
  PIDS+=($!)
fi
if ! nc -z 127.0.0.1 4445 2>/dev/null; then
  EZH_ADAPTER_SERIAL="$BOARD_B_SERIAL" EZH_GDB_PORT=3334 EZH_TELNET_PORT=4445 \
    openocd -f "$CFG" >/tmp/ezh_ocd_boardB.log 2>&1 &
  PIDS+=($!)
fi
[ ${#PIDS[@]} -gt 0 ] && trap 'kill ${PIDS[*]} 2>/dev/null || true' EXIT && sleep 3

export EZH_READELF="$B/llvm-readelf"
python3 - <<'PY'
import os,sys,time,subprocess
sys.path.insert(0, os.getcwd())
from ezh_run_telnet import OCD
A=os.path.abspath
RE=os.environ["EZH_READELF"]
def syms(elf,names):
    d={}
    for L in subprocess.check_output([RE,"-s",elf]).decode().splitlines():
        f=L.split()
        if len(f)>=8 and f[7] in names: d[f[7]]=int(f[1],16)
    return d
def rd(o,x):
    for _ in range(6):
        v=o.mdw("0x%08x"%x)
        if v is not None: return v&0xffffffff
        o.cmd("poll off"); time.sleep(0.08)
    return -1
def pokes(o):
    o.cmd("halt")
    o.cmd("mww 0x40001040 0x40000000");o.cmd("mww 0x40002634 0x00030000");o.cmd("mww 0x4013500C 0x1")
    o.cmd("mww 0x40000040 0x40000000");o.cmd("mww 0x40000070 0x40000000")
def start(o,elf):
    o.cmd("mww 0x40027024 0xC0DE0000");o.cmd("load_image %s"%elf,settle=0.7)
    o.cmd("mww 0x40027024 0xC0DE0010");o.cmd("mww 0x40027020 0x24100000");o.cmd("mww 0x40027024 0xC0DE0011")
def i3c_common_init(o):
    o.cmd("mww 0x40001110 0x0000001F")                             # FRODIVOEN
    cur=rd(o,0x40021018); o.cmd("mww 0x40021018 0x%08x"%(cur|0x10000))  # gate I3C0 clock
    o.cmd("mww 0x40020048 0x00010000"); time.sleep(0.02)           # I3C0 reset assert
    o.cmd("mww 0x40020078 0x00010000"); time.sleep(0.02)           # deassert (FSM init)
    o.cmd("mww 0x40021800 0x1")                                    # FCLKSEL = FRO_DIV8 (24MHz)
    o.cmd("mww 0x40021810 0x20000000"); o.cmd("mww 0x40021810 0x0")# FCLKDIV pulse -> div1
    o.cmd("mww 0x40021804 0x1")                                    # STCSEL = LPOSC 1MHz (CCC/DAA engine)
    o.cmd("mww 0x40021808 0x20000000"); o.cmd("mww 0x40021808 0x0")# STCDIV pulse -> div1
    o.cmd("mww 0x4002180C 0x20000000"); o.cmd("mww 0x4002180C 0x0")# SLOWDIV pulse -> div1
    o.cmd("mww 0x40004174 0x000001C1"); o.cmd("mww 0x40004178 0x000001C1")  # PIO2_29/30, full drive
    o.cmd("mww 0x4000417C 0x00000181")                             # PIO2_31 = I3C0_PUR, full drive
    o.cmd("mww 0x40021048 0x80000000")                             # INPUTMUX clock
    o.cmd("mww 0x40026720 0x0000001A")                             # trig ch0 = 26 (I3c0Irq)
def i3c_master_init(o):
    i3c_common_init(o)
    o.cmd("mww 0x40036000 0x01040001")                             # MCONFIG: OD 2.4MHz, PP 12MHz
    o.cmd("mww 0x400360AC 0x0000003B")                             # MDATACTRL: UNLOCK|flush
def i3c_slave_init(o):
    i3c_common_init(o)
    o.cmd("mww 0x4003606C 0xCAFE1234")                             # SIDPARTNO
    o.cmd("mww 0x40036068 0x00FF00FF")                             # SMAXLIMITS
    o.cmd("mww 0x40036004 0x84010019")                             # SCONFIG: SADDR 0x42|BAMATCH 1|DDROK|S0IGNORE|SLVENA
    o.cmd("mww 0x4003602C 0x0000000B")                             # SDATACTRL: UNLOCK|flush
    o.cmd("mww 0x40036010 0x00002C00")                             # SINTSET: DACHG|RXPEND|STOP
    o.cmd("mww 0x40027048 0x00000080")                             # PENDTRAP hygiene

SL=A("out_i3c_pre/i3c_preempt_slave.elf"); MA=A("out_i3c_pre/i3c_sdr_master.elf")
S=syms(SL,{"s_buf","s_isr","s_rx","s_stops","s_dachg","s_dyn","s_tiles","s_tiles_at_done","tile_dst","exc_signal"})
M=syms(MA,{"m_stage","m_nacked","m_holds","m_complete","exc_signal"})
EXPECT=[0x00]+[0x10+i for i in range(16)]
TILE_EXPECT=[(0xA0+i)&0xff for i in range(3*16)]

print("=== PREEMPTIVE MODEL: tile-copy mainline, I3C ISR in vector0() ===")
a=OCD(4444); a.cmd("poll off"); pokes(a); i3c_master_init(a)
b=OCD(4445); b.cmd("poll off"); pokes(b); i3c_slave_init(b)
time.sleep(0.3)
start(b,SL); time.sleep(0.8)          # slave tiling away, ISR armed
t_before=rd(b,S["s_tiles"])
start(a,MA); time.sleep(3.0)          # the whole I3C ceremony preempts it
m={k:rd(a,M[k]) for k in M}
del a; time.sleep(0.3)
exc=rd(b,S["exc_signal"])
v={k:rd(b,S[k]) for k in ("s_isr","s_rx","s_stops","s_dachg","s_dyn","s_tiles","s_tiles_at_done")}
got=[rd(b,S["s_buf"]+i)&0xff for i in range(len(EXPECT))]
tiles=[rd(b,S["tile_dst"]+i)&0xff for i in range(3*16)]

print("  MASTER exc=0x%08x stage=%d nacked=0x%x complete=%d"%(m["exc_signal"],m["m_stage"],m["m_nacked"],m["m_complete"]))
print("  SLAVE  exc=0x%08x | isr entries=%d"%(exc,v["s_isr"]))
print("         i3c: rx=%d stops=%d dachg=%d dynaddr=0x%02x"%(v["s_rx"],v["s_stops"],v["s_dachg"],v["s_dyn"]))
print("         tiles: %d copied total (%d before master, %d at payload-done)"%(v["s_tiles"],t_before,v["s_tiles_at_done"]))
print("         buf=%s"%" ".join("%02x"%x for x in got))
ok = (exc==0xCAFEBABE and m["exc_signal"]==0xCAFEBABE and m["m_nacked"]==0 and m["m_complete"]==1
      and got==EXPECT and v["s_rx"]==len(EXPECT) and v["s_stops"]>=1 and v["s_dyn"]==0x61
      and v["s_isr"]>=3 and v["s_tiles"]>=50 and tiles==TILE_EXPECT)
if got!=EXPECT: print("         BUF MISMATCH exp %s"%" ".join("%02x"%x for x in EXPECT))
if tiles!=TILE_EXPECT: print("         TILE MISMATCH")
print("\n>>> PREEMPTIVE-MODEL I3C %s <<<"%("PASSED" if ok else "FAILED"))
sys.exit(0 if ok else 1)
PY
