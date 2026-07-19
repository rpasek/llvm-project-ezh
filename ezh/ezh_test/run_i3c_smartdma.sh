#!/bin/bash
# HW-I2C via SmartDMA: two-board harness that drives the on-chip I3C0 peripheral
# in legacy-I2C master mode from the EZH, talking to the board-B bit-bang
# register-map slave over the already-wired I3C header (PIO2_29=SCL/PIO2_30=SDA).
#
# Runs three stages, each validated on silicon:
#   M0b  i3c_hw_master  -- poll-based transaction (5-byte write)
#   M1   i3c_stream     -- event-paced streaming of 16 bytes (2x the FIFO) via
#                          __builtin_ezh_hold on the I3C0 TX-ready event
#   M1b  i3c_tight_loop -- the LITERAL tight_loop (OP_LOOP) hardware loop
#
# Built with Richard Pasek's reorganised EZH backend (worktree
# /Users/foxy/Downloads/llvm-ezh-port); like run_i2c_regmap_hisbackend.sh we link
# a tiny minrt shim instead of the prebuilt runtimes. Slave on board B (:4445),
# I3C0 master on board A (:4444).
#
# See I3C_SMARTDMA.md for the register-level bring-up (esp. the peripheral-reset
# pulse) and how tight_loop works.
set -e
cd "$(dirname "$0")"
WT="$(cd ../.. && pwd)"   # repo root: this script lives in ezh/ezh_test/
B=$WT/build/bin
# Two boards, wired on the I3C header (PIO2_29=SCL, PIO2_30=SDA, common ground).
# Set both probe serials -- find them with:  ls /dev/cu.usbmodem*  (macOS)  or
#   ls /dev/serial/by-id/  /  lsusb -d 1fc9: -v | grep iSerial  (Linux).
: "${BOARD_A_SERIAL:?set BOARD_A_SERIAL to the I3C0-master board's probe serial}"
: "${BOARD_B_SERIAL:?set BOARD_B_SERIAL to the bit-bang-slave board's probe serial}"
F="-target ezh-none-elf -mno-ezh-bitslice-interrupts -Os -ffreestanding -ffunction-sections -fdata-sections"
mkdir -p out_i3c; cd out_i3c

cat > minrt.c <<'EOF'
unsigned __mulsi3(unsigned a,unsigned b){unsigned r=0;while(b){if(b&1)r+=a;a<<=1;b>>=1;}return r;}
unsigned __udivsi3(unsigned a,unsigned b){unsigned q=0,r=0;for(int i=31;i>=0;i--){r=(r<<1)|((a>>i)&1);if(r>=b){r-=b;q|=1u<<i;}}return q;}
unsigned __umodsi3(unsigned a,unsigned b){unsigned r=0;for(int i=31;i>=0;i--){r=(r<<1)|((a>>i)&1);if(r>=b)r-=b;}return r;}
EOF
$B/clang $F -D__TEST__ -DSTACK_SIZE_WORDS=1280 -I $WT/ezh -I $WT/lldb/source/Plugins/Process/EZH $WT/ezh/crt0.c -c -o crt0.o
$B/clang $F minrt.c -c -o minrt.o
$B/clang $F -I .. -I ../.. ../ezh_i2c_slave_regmap.c -c -o slave.o
$B/ld.lld -T ../../smartdma.ld --gc-sections --discard-locals crt0.o slave.o minrt.o -o slave.elf
for t in i3c_hw_master i3c_stream i3c_tight_loop; do
  $B/clang $F -I .. -I ../.. ../$t.c -c -o $t.o
  $B/ld.lld -T ../../smartdma.ld --gc-sections --discard-locals crt0.o $t.o minrt.o -o $t.elf
done
echo "built out_i3c/{slave,i3c_hw_master,i3c_stream,i3c_tight_loop}.elf"
cd ..

# Start OpenOCD for both boards if not already up: master on the default ports
# (gdb 3333 / telnet 4444), slave on gdb 3334 / telnet 4445.
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
SL=A("out_i3c/slave.elf")
S=syms(SL,{"regmap","i2c_wr_count"})
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
def i3c_init(o):
    # --- HW I3C0 legacy-I2C master bring-up (M33) -> STATE=IDLE ---
    o.cmd("mww 0x40001110 0x0000001F")                             # FRODIVOEN: FRO divided outputs
    cur=rd(o,0x40021018); o.cmd("mww 0x40021018 0x%08x"%(cur|0x10000))  # gate I3C0 clock
    o.cmd("mww 0x40020048 0x00010000"); time.sleep(0.02)           # PRSTCTL2_SET: assert I3C0 reset
    o.cmd("mww 0x40020078 0x00010000"); time.sleep(0.02)           # PRSTCTL2_CLR: deassert -> FSM init *** the key step ***
    o.cmd("mww 0x40021800 0x1")                                    # FCLKSEL = FRO_DIV8 (24MHz)
    o.cmd("mww 0x40021810 0x20000000"); o.cmd("mww 0x40021810 0x0")# FCLKDIV reset-pulse -> div1
    o.cmd("mww 0x40021804 0x0"); o.cmd("mww 0x40021808 0x0")       # STCSEL/STCDIV: timing-control clk from FCLK
    o.cmd("mww 0x40004174 0x00000071"); o.cmd("mww 0x40004178 0x00000071")  # PIO2_29/30 = I3C0 SCL/SDA
    o.cmd("mww 0x4000417C 0x00000001")                             # PIO2_31 = I3C0_PUR
    o.cmd("mww 0x40036000 0x03280249")                             # MCONFIG: MSTENA|DISTO|ODSTOP|ODHPP|SKEW + ~100kHz baud
    o.cmd("mww 0x400360AC 0x0000003B")                             # MDATACTRL: UNLOCK|TXTRIG|flush
    # --- event-pacing wiring (used by i3c_stream / i3c_tight_loop) ---
    o.cmd("mww 0x40036090 0x00001000")                             # MINTSET.TXNOTFULL -> IRQ while TX has room
    o.cmd("mww 0x40021048 0x80000000")                             # INPUTMUX clock (PSCCTL2_SET bit31)
    o.cmd("mww 0x40026720 0x0000001A")                             # SMART_DMA_TRIG_CH_SEL[0] = 26 (I3c0Irq -> trig ch0)

def run(name, elf, nregs, expect, msyms):
    elf=A(elf)
    M=syms(elf,msyms)
    # board B: slave, cleared regmap window
    b=OCD(4445); b.cmd("poll off"); pokes(b)
    for i in range(nregs): b.cmd("mwb 0x%08x 0x00"%(S["regmap"]+i))
    start(b,SL); time.sleep(1.0); del b; time.sleep(0.5)
    # board A: I3C init then the master firmware
    a=OCD(4444); a.cmd("poll off"); pokes(a); i3c_init(a)
    idle=rd(a,0x40036088)
    start(a,elf); time.sleep(2.5)
    m={k:rd(a,M[k]) for k in M}; del a; time.sleep(0.5)
    # board B: read back the register map
    b=OCD(4445); b.cmd("poll off")
    got=[rd(b,S["regmap"]+i)&0xff for i in range(nregs)]
    wr=rd(b,S["i2c_wr_count"])
    ok = got==expect
    print("  [%s] idle_state=%d %s"%(name, idle&7, " ".join("%s=%d"%(k,v) for k,v in m.items())))
    print("       regmap=%s wr=%d %s"%(" ".join("%02x"%x for x in got),wr,"OK" if ok else "MISMATCH exp %s"%(" ".join("%02x"%x for x in expect))))
    return ok

print("=== HW-I2C via SmartDMA (I3C0 legacy master, driven by the EZH) ===")
r0=run("M0b  poll     ", "out_i3c/i3c_hw_master.elf", 12, [0]*8+[0x11,0x22,0x33,0x44],
       {"m_mctrldone","m_complete"})
r1=run("M1   stream   ", "out_i3c/i3c_stream.elf", 16, [0x10+i for i in range(16)],
       {"m_mctrldone","m_holds","m_complete"})
r2=run("M1b  tight_loop", "out_i3c/i3c_tight_loop.elf", 7, [0x10+i for i in range(7)],
       {"m_mctrldone","m_rend","m_complete"})
print("\n>>> %s <<<"%("ALL PASSED" if (r0 and r1 and r2) else "FAILED: M0b=%s M1=%s M1b=%s"%(r0,r1,r2)))
sys.exit(0 if (r0 and r1 and r2) else 1)
PY
