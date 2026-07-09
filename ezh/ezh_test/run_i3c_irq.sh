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
# Fully INTERRUPT-DRIVEN two-board I2C -- no polling on either data path.
#
#   board A (master): i3c_stream.c   -- HW I3C0 master, each TX byte gated by
#                                       __builtin_ezh_hold() on the TXNOTFULL IRQ
#   board B (slave):  i3c_slave_irq.c -- HW I3C0 in I2C-legacy SLAVE mode
#                                       (static addr 0x42); the EZH sleeps in
#                                       __builtin_ezh_hold() and is woken per
#                                       RXPEND/STOP interrupt to drain the FIFO
#
# Same J18 I3C-header wiring as the other demos (PIO2_29=SCL, PIO2_30=SDA,
# common ground) -- both boards now use the pins in I3C peripheral function.
# 17 bytes (0x00, 0x10..0x1F) cross the bus; the slave's wake counter proves it
# slept between events instead of spinning.
#===----------------------------------------------------------------------===#
set -e
cd "$(dirname "$0")"
WT="$(cd ../.. && pwd)"
B=$WT/build/bin
# Two boards wired on the I3C header. Find serials: ls /dev/cu.usbmodem* (macOS)
# or ls /dev/serial/by-id/ (Linux).
: "${BOARD_A_SERIAL:?set BOARD_A_SERIAL to the master board's probe serial}"
: "${BOARD_B_SERIAL:?set BOARD_B_SERIAL to the slave board's probe serial}"
F="-target ezh-none-elf -mno-ezh-bitslice-interrupts -Os -ffreestanding -ffunction-sections -fdata-sections"
mkdir -p out_i3c_irq; cd out_i3c_irq

cat > minrt.c <<'EOF'
unsigned __mulsi3(unsigned a,unsigned b){unsigned r=0;while(b){if(b&1)r+=a;a<<=1;b>>=1;}return r;}
unsigned __udivsi3(unsigned a,unsigned b){unsigned q=0,r=0;for(int i=31;i>=0;i--){r=(r<<1)|((a>>i)&1);if(r>=b){r-=b;q|=1u<<i;}}return q;}
unsigned __umodsi3(unsigned a,unsigned b){unsigned r=0;for(int i=31;i>=0;i--){r=(r<<1)|((a>>i)&1);if(r>=b)r-=b;}return r;}
EOF
$B/clang $F -D__TEST__ -DSTACK_SIZE_WORDS=1280 -I $WT/ezh -I $WT/lldb/source/Plugins/Process/EZH $WT/ezh/crt0.c -c -o crt0.o
$B/clang $F minrt.c -c -o minrt.o
for t in i3c_slave_irq i3c_stream; do
  $B/clang $F -I .. -I ../.. ../$t.c -c -o $t.o
  $B/ld.lld -T ../../smartdma.ld --gc-sections --discard-locals crt0.o $t.o minrt.o -o $t.elf
done
echo "built out_i3c_irq/{i3c_slave_irq,i3c_stream}.elf"
cd ..

# Start OpenOCD for both boards if not already up: master A on gdb 3333 /
# telnet 4444, slave B on gdb 3334 / telnet 4445.
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
    # clocks + the peripheral-reset pulse (the FSM does not run without it)
    o.cmd("mww 0x40001110 0x0000001F")                             # FRODIVOEN
    cur=rd(o,0x40021018); o.cmd("mww 0x40021018 0x%08x"%(cur|0x10000))  # gate I3C0 clock
    o.cmd("mww 0x40020048 0x00010000"); time.sleep(0.02)           # PRSTCTL2_SET: assert reset
    o.cmd("mww 0x40020078 0x00010000"); time.sleep(0.02)           # PRSTCTL2_CLR: deassert
    o.cmd("mww 0x40021800 0x1")                                    # FCLKSEL = FRO_DIV8 (24MHz)
    o.cmd("mww 0x40021810 0x20000000"); o.cmd("mww 0x40021810 0x0")# FCLKDIV pulse -> div1
    o.cmd("mww 0x40021804 0x0"); o.cmd("mww 0x40021808 0x0")       # STC clk
    o.cmd("mww 0x40004174 0x00000071"); o.cmd("mww 0x40004178 0x00000071")  # PIO2_29/30 = I3C0
    o.cmd("mww 0x40021048 0x80000000")                             # INPUTMUX clock
    o.cmd("mww 0x40026720 0x0000001A")                             # trig ch0 = 26 (I3c0Irq)
def i3c_master_init(o):
    i3c_common_init(o)
    o.cmd("mww 0x4000417C 0x00000001")                             # PIO2_31 = I3C0_PUR
    o.cmd("mww 0x40036000 0x03280249")                             # MCONFIG: master, ~100kHz
    o.cmd("mww 0x400360AC 0x0000003B")                             # MDATACTRL: UNLOCK|flush
    o.cmd("mww 0x40036090 0x00001000")                             # MINTSET.TXNOTFULL -> IRQ
def i3c_slave_init(o):
    i3c_common_init(o)
    # SCONFIG: SADDR(0x42)<<25 | BAMATCH(24 = FCLK MHz)<<16 | SLVENA
    o.cmd("mww 0x40036004 0x84180001")
    o.cmd("mww 0x4003602C 0x0000000B")                             # SDATACTRL: UNLOCK|flush
    o.cmd("mww 0x40036010 0x00000C00")                             # SINTSET: RXPEND|STOP

SL=A("out_i3c_irq/i3c_slave_irq.elf"); MA=A("out_i3c_irq/i3c_stream.elf")
S=syms(SL,{"s_buf","s_holds","s_rx","s_stops","exc_signal"})
M=syms(MA,{"m_mctrldone","m_holds","m_complete","exc_signal"})
EXPECT=[0x00]+[0x10+i for i in range(16)]      # what i3c_stream puts on the wire

print("=== interrupt-driven I2C: HW-I3C slave (hold/IRQ) <- event-paced master ===")
# board B: bring up + arm + ignite the IRQ slave FIRST (it sleeps in hold)
b=OCD(4445); b.cmd("poll off"); pokes(b); i3c_slave_init(b)
start(b,SL); time.sleep(1.2)
h0=rd(b,S["s_holds"])
del b; time.sleep(0.4)
# board A: bring up + ignite the event-paced master
a=OCD(4444); a.cmd("poll off"); pokes(a); i3c_master_init(a)
start(a,MA); time.sleep(3.0)
m={k:rd(a,M[k]) for k in ("m_mctrldone","m_holds","m_complete","exc_signal")}
del a; time.sleep(0.4)
# board B: read the outcome
b=OCD(4445); b.cmd("poll off")
exc=rd(b,S["exc_signal"]); holds=rd(b,S["s_holds"]); nrx=rd(b,S["s_rx"]); stops=rd(b,S["s_stops"])
got=[rd(b,S["s_buf"]+i)&0xff for i in range(len(EXPECT))]

print("  MASTER exc=0x%08x mctrldone=%d tx_holds=%d complete=%d"%(m["exc_signal"],m["m_mctrldone"],m["m_holds"],m["m_complete"]))
print("  SLAVE  exc=0x%08x rx=%d stops=%d wakes=%d (holds before master ran: %d)"%(exc,nrx,stops,holds,h0))
print("         buf=%s"%" ".join("%02x"%x for x in got))
ok = (exc==0xCAFEBABE and got==EXPECT and nrx==len(EXPECT) and stops>=1
      and m["m_complete"]==1 and m["exc_signal"]==0xCAFEBABE and holds>=2)
if got!=EXPECT: print("         MISMATCH exp %s"%" ".join("%02x"%x for x in EXPECT))
print("\n>>> INTERRUPT-DRIVEN I2C %s <<<"%("PASSED" if ok else "FAILED"))
sys.exit(0 if ok else 1)
PY
