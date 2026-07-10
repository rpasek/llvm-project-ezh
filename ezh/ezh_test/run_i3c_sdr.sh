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
# TRUE I3C (SDR) between two boards, fully interrupt-driven on both EZH cores.
#
#   board A (master): i3c_sdr_master.c -- real ENTDAA assigns dynamic address
#                     0x30 (the master reads the slave's 8 DAA ID bytes), then
#                     a private SDR write of 17 bytes; every phase gated by
#                     __builtin_ezh_hold() on MCTRLDONE/RXPEND/TXNOTFULL/
#                     COMPLETE interrupts.
#   board B (slave):  i3c_sdr_slave.c -- sleeps in hold; DACHG interrupt
#                     captures the dynamic-address assignment (SDYNADDR),
#                     RXPEND/STOP interrupts deliver the payload.
#
# Same J18 wiring as the other demos. Native I3C SDR: open-drain headers at
# ~100 kHz, push-pull data at ~4 MHz (MCONFIG PPBAUD=2 @ 24 MHz FCLK).
#===----------------------------------------------------------------------===#
set -e
cd "$(dirname "$0")"
WT="$(cd ../.. && pwd)"
B=$WT/build/bin
: "${BOARD_A_SERIAL:?set BOARD_A_SERIAL to the master board's probe serial}"
: "${BOARD_B_SERIAL:?set BOARD_B_SERIAL to the slave board's probe serial}"
F="-target ezh-none-elf -mno-ezh-bitslice-interrupts -Os -ffreestanding -ffunction-sections -fdata-sections"
mkdir -p out_i3c_sdr; cd out_i3c_sdr

cat > minrt.c <<'EOF'
unsigned __mulsi3(unsigned a,unsigned b){unsigned r=0;while(b){if(b&1)r+=a;a<<=1;b>>=1;}return r;}
unsigned __udivsi3(unsigned a,unsigned b){unsigned q=0,r=0;for(int i=31;i>=0;i--){r=(r<<1)|((a>>i)&1);if(r>=b){r-=b;q|=1u<<i;}}return q;}
unsigned __umodsi3(unsigned a,unsigned b){unsigned r=0;for(int i=31;i>=0;i--){r=(r<<1)|((a>>i)&1);if(r>=b)r-=b;}return r;}
EOF
$B/clang $F -D__TEST__ -DSTACK_SIZE_WORDS=1280 -I $WT/ezh -I $WT/lldb/source/Plugins/Process/EZH $WT/ezh/crt0.c -c -o crt0.o
$B/clang $F minrt.c -c -o minrt.o
for t in i3c_sdr_slave i3c_sdr_master; do
  $B/clang $F -I .. -I ../.. ../$t.c -c -o $t.o
  $B/ld.lld -T ../../smartdma.ld --gc-sections --discard-locals crt0.o $t.o minrt.o -o $t.elf
done
echo "built out_i3c_sdr/{i3c_sdr_slave,i3c_sdr_master}.elf"
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
    # The I3C time-control (slow) clock: the CCC/DAA engine is dead without it.
    # LPOSC 1MHz -> TC, both TC and SLOW dividers unhalted at div1.
    o.cmd("mww 0x40021804 0x1")                                    # STCSEL = LPOSC 1MHz
    o.cmd("mww 0x40021808 0x20000000"); o.cmd("mww 0x40021808 0x0")# STCDIV pulse -> div1
    o.cmd("mww 0x4002180C 0x20000000"); o.cmd("mww 0x4002180C 0x0")# SLOWDIV pulse -> div1
    # Pins MUST be full-drive + slew, no internal pulls (0x1C1; PUR 0x181).
    # With the weak-drive 0x71 config private SDR writes still work, but the
    # PUR-dependent open-drain turnarounds of every CCC/DAA get corrupted: the
    # slave ACKs 0x7E and then never sees the CCC. (Diffed from the live
    # register state of NXP's working i3c_interrupt_b2b example.)
    o.cmd("mww 0x40004174 0x000001C1"); o.cmd("mww 0x40004178 0x000001C1")  # PIO2_29/30 = I3C0, full drive
    o.cmd("mww 0x4000417C 0x00000181")                             # PIO2_31 = I3C0_PUR, full drive
    o.cmd("mww 0x40021048 0x80000000")                             # INPUTMUX clock
    o.cmd("mww 0x40026720 0x0000001A")                             # trig ch0 = 26 (I3c0Irq)
def i3c_master_init(o):
    i3c_common_init(o)
    # ODHPP=1, ODBAUD=4 -> open-drain ~2.4 MHz, PPBAUD=0 -> push-pull ~12 MHz
    # (the NXP-example rates; proven on this wiring)
    o.cmd("mww 0x40036000 0x01040001")                             # MCONFIG: OD 2.4MHz, PP 12MHz
    o.cmd("mww 0x400360AC 0x0000003B")                             # MDATACTRL: UNLOCK|flush
    # NOTE: no MINTSET here -- the interrupt-driven master firmware drives it
def i3c_slave_init(o):
    i3c_common_init(o)
    o.cmd("mww 0x4003606C 0xCAFE1234")                             # SIDPARTNO: deterministic part-no
    o.cmd("mww 0x40036068 0x00FF00FF")                             # SMAXLIMITS: MAXRD/MAXWR 255
    # BAMATCH counts the SLOW clock: 1MHz -> 1 (SDK formula, clamped to >=1)
    o.cmd("mww 0x40036004 0x84010019")                             # SCONFIG: SADDR 0x42|BAMATCH 1|DDROK|S0IGNORE|SLVENA
    o.cmd("mww 0x4003602C 0x0000000B")                             # SDATACTRL: UNLOCK|flush
    o.cmd("mww 0x40036010 0x00002C00")                             # SINTSET: DACHG|RXPEND|STOP

SL=A("out_i3c_sdr/i3c_sdr_slave.elf"); MA=A("out_i3c_sdr/i3c_sdr_master.elf")
S=syms(SL,{"s_buf","s_holds","s_rx","s_stops","s_dachg","s_dyn","exc_signal"})
M=syms(MA,{"m_stage","m_nacked","m_holds","m_complete","m_pid_lo","m_pid_hi","m_bcrdcr","exc_signal"})
EXPECT=[0x00]+[0x10+i for i in range(16)]
DYN_EXPECT=(0x30<<1)|1   # SDYNADDR: DADDR | DAVALID

print("=== TRUE I3C SDR: ENTDAA + private write, interrupt-driven both ends ===")
# Bring up BOTH I3C blocks first (master's PUR pulls the bus idle-high) so the
# slave is configured against a clean bus -- a slave born mid-message latches
# STNOTSTOP and refuses to engage.
a=OCD(4444); a.cmd("poll off"); pokes(a); i3c_master_init(a)
b=OCD(4445); b.cmd("poll off"); pokes(b); i3c_slave_init(b)
time.sleep(0.3)
start(b,SL); time.sleep(1.2)   # slave first: asleep in hold, armed
del b; time.sleep(0.4)
start(a,MA); time.sleep(3.0)   # then the master runs the DAA + write
m={k:rd(a,M[k]) for k in M}
del a; time.sleep(0.4)
b=OCD(4445); b.cmd("poll off")
exc=rd(b,S["exc_signal"]); holds=rd(b,S["s_holds"]); nrx=rd(b,S["s_rx"])
stops=rd(b,S["s_stops"]); dachg=rd(b,S["s_dachg"]); dyn=rd(b,S["s_dyn"])
got=[rd(b,S["s_buf"]+i)&0xff for i in range(len(EXPECT))]

print("  MASTER exc=0x%08x stage=%d nacked=0x%x holds=%d complete=%d"%(m["exc_signal"],m["m_stage"],m["m_nacked"],m["m_holds"],m["m_complete"]))
print("         DAA read slave ID: PID=%04x%08x BCR/DCR=%04x"%(m["m_pid_hi"],m["m_pid_lo"],m["m_bcrdcr"]))
print("  SLAVE  exc=0x%08x dynaddr=0x%02x (dachg=%d) rx=%d stops=%d wakes=%d"%(exc,dyn,dachg,nrx,stops,holds))
print("         buf=%s"%" ".join("%02x"%x for x in got))
ok = (exc==0xCAFEBABE and m["exc_signal"]==0xCAFEBABE and m["m_nacked"]==0
      and m["m_stage"]==4 and m["m_complete"]==1
      and dyn==DYN_EXPECT and dachg>=1 and got==EXPECT and nrx==len(EXPECT) and stops>=1)
if got!=EXPECT: print("         MISMATCH exp %s"%" ".join("%02x"%x for x in EXPECT))
if dyn!=DYN_EXPECT: print("         DYNADDR MISMATCH exp 0x%02x"%DYN_EXPECT)
print("\n>>> TRUE-I3C (SDR) INTERRUPT-DRIVEN %s <<<"%("PASSED" if ok else "FAILED"))
sys.exit(0 if ok else 1)
PY
