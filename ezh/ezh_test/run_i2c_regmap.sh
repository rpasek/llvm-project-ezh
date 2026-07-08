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
# Two-board EZH I2C register-map test: the free-running, register-mapped slave
# (ezh_i2c_slave_regmap.c) on board A, the register-map master
# (ezh_i2c_master_regmap.c) on board B, wired J18<->J18 (SDA=PIO2_30/bit30,
# SCL=PIO2_29/bit29) with external pull-ups. Exercises all three address forms
# the slave decodes: 7-bit (0x42), general call (0x00 broadcast + reset), and
# 10-bit (0x242, combined-format read).
#
# Board A (slave): assumed already served by OpenOCD on :4444 (the working probe
#   brought up once this power cycle). Board B (master): this script starts its
#   own OpenOCD on :4445 using EZH_ADAPTER_SERIAL=$BOARD_B_SERIAL.
#
# The slave runs forever (no exc_signal); both EZH cores are ignited directly
# over the OpenOCD telnet console -- including the RSTCTL reset pulse that brings
# a freshly-attached board's SmartDMA control block out of reset (without it the
# CTRL/BOOTADR registers ignore writes). Success is checked by reading the
# slave's regmap + counters and the master's self-verified read-back.
#===----------------------------------------------------------------------===#
set -e
cd "$(dirname "$0")"

# Two boards wired on the I3C header (SDA=PIO2_30, SCL=PIO2_29) with pull-ups.
# Set both probe serials (ls /dev/cu.usbmodem*  or  ls /dev/serial/by-id/).
: "${BOARD_A_SERIAL:?set BOARD_A_SERIAL to the slave board's probe serial}"
: "${BOARD_B_SERIAL:?set BOARD_B_SERIAL to the master board's probe serial}"
BUILD=../../build/bin
OCD_CFG=../rt595-openocd.cfg

echo "=== 1. Build the register-map slave + master ==="
mkdir -p out
$BUILD/clang -target ezh-none-elf -mno-ezh-bitslice-interrupts -g -Os \
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror \
    -isystem ../../build/libc/libc/include -I ../../lldb/source/Plugins/Process/EZH/ \
    -D__TEST__ -DSTACK_SIZE_WORDS=1280 ../crt0.c -c -o out/crt0.o -fno-builtin 2>/dev/null || \
$BUILD/clang -target ezh-none-elf -mno-ezh-bitslice-interrupts -Os \
    -isystem ../../build/libc/libc/include -I ../../libc ../crt0.c -c -o out/crt0.o 2>/dev/null || true
for t in ezh_i2c_slave_regmap ezh_i2c_master_regmap; do
    $BUILD/clang -target ezh-none-elf -mno-ezh-bitslice-interrupts -Os \
        -ffunction-sections -fdata-sections -Wall -Wextra -Werror \
        -isystem ../../build/libc/libc/include -I ../../libc -I . -I .. \
        $t.c -c -o out/$t.o
    $BUILD/ld.lld -T ../smartdma.ld --gc-sections out/crt0.o out/$t.o \
        ../../build/libc/libc/lib/libc_nano.a \
        ../../build/compiler-rt/lib/linux/libclang_rt.builtins-ezh.a \
        -o out/$t.elf
done
echo "    built out/ezh_i2c_slave_regmap.elf, out/ezh_i2c_master_regmap.elf"

echo "=== 2. Start OpenOCD for both boards (A slave :4444, B master :4445) ==="
_PIDS=()
if ! nc -z 127.0.0.1 4444 2>/dev/null; then
  EZH_ADAPTER_SERIAL="$BOARD_A_SERIAL" \
      openocd -f "$OCD_CFG" >/tmp/ezh_ocd_boardA.log 2>&1 &
  _PIDS+=($!)
fi
if ! nc -z 127.0.0.1 4445 2>/dev/null; then
  EZH_ADAPTER_SERIAL="$BOARD_B_SERIAL" EZH_GDB_PORT=3334 EZH_TELNET_PORT=4445 \
      openocd -f "$OCD_CFG" >/tmp/ezh_ocd_boardB.log 2>&1 &
  _PIDS+=($!)
fi
[ ${#_PIDS[@]} -gt 0 ] && trap 'kill ${_PIDS[*]} 2>/dev/null || true' EXIT && sleep 3

echo "=== 3. Ignite slave (A:4444) + master (B:4445); read results ==="
python3 - <<'PY'
import os,sys,time,subprocess
sys.path.insert(0, os.getcwd())
from ezh_run_telnet import OCD
B=os.path.abspath
SL=B("out/ezh_i2c_slave_regmap.elf"); MA=B("out/ezh_i2c_master_regmap.elf")
def syms(elf,names):
    d={}
    for L in subprocess.check_output(["../../build/bin/llvm-readelf","-s",elf]).decode().splitlines():
        f=L.split()
        if len(f)>=8 and f[7] in names: d[f[7]]=int(f[1],16)
    return d
S=syms(SL,{"regmap","i2c_wr_count","i2c_rd_count","i2c_txn_count","i2c_gc_count","i2c_gc_reset"})
M=syms(MA,{"m_status","m_rb_7bit","m_rb_gc","m_rb_10bit"})
a=OCD(4444); b=OCD(4445)
def ignite(o,elf):
    o.cmd("halt")
    o.cmd("mww 0x40001040 0x40000000");o.cmd("mww 0x40002634 0x00030000");o.cmd("mww 0x4013500C 0x1")
    o.cmd("mww 0x40000040 0x40000000");o.cmd("mww 0x40000070 0x40000000")  # SmartDMA reset pulse
    o.cmd("mww 0x40027024 0xC0DE0000");o.cmd("load_image %s"%elf,settle=0.7)
    o.cmd("mww 0x40027024 0xC0DE0010");o.cmd("mww 0x40027020 0x24100000");o.cmd("mww 0x40027024 0xC0DE0011")
def rd(o,x): v=o.mdw("0x%08x"%x); return v if v is not None else -1
ignite(a,SL); time.sleep(1.0)
ignite(b,MA); time.sleep(3.0)
base=S["regmap"]
w8=rd(a,base+8); w0=rd(a,base+0); w20=rd(a,base+20); w12=rd(a,base+12)
ms=rd(b,M["m_status"]); gr=rd(a,S["i2c_gc_reset"])
print("  SLAVE  7bit regmap[8..11]=0x%08x [0..3]=0x%08x | GC regmap[20..]=0x%04x | 10bit regmap[12..]=0x%04x"%(w8,w0,w20&0xFFFF,w12&0xFFFF))
print("         wr=%d rd=%d txn=%d gc_count=%d gc_reset=%d"%(rd(a,S["i2c_wr_count"]),rd(a,S["i2c_rd_count"]),rd(a,S["i2c_txn_count"]),rd(a,S["i2c_gc_count"]),gr))
print("  MASTER m_status=0x%08x rb_7bit=0x%08x rb_gc=0x%04x rb_10bit=0x%04x"%(ms,rd(b,M["m_rb_7bit"]),rd(b,M["m_rb_gc"])&0xFFFF,rd(b,M["m_rb_10bit"])&0xFFFF))
ok=(ms==0 and w8==0x44332211 and (w20&0xFFFF)==0xADDE and (w12&0xFFFF)==0xFECA and gr>=1)
print("\n>>> REGMAP I2C (7-bit + general-call + 10-bit) %s <<<"%("PASSED" if ok else "FAILED"))
sys.exit(0 if ok else 1)
PY
