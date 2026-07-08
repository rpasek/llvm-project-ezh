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
# Two-board EZH I2C loopback: slave on board A, master on board B, wired
# J18<->J18 (SDA=PIO2_30/bit30, SCL=PIO2_29/bit29) with external pull-ups.
#
# Board A (slave): assumed already served by OpenOCD on :3333 / telnet :4444,
#   started with the WORKING probe and brought up once this power cycle:
#     EZH_ADAPTER_SERIAL=<A> openocd -f ../rt595-openocd.cfg
# Board B (master): this script starts its own OpenOCD on :3334 / telnet :4445
#   using EZH_ADAPTER_SERIAL=$BOARD_B_SERIAL and brings up its SMARTDMA block.
#
# Both EZH firmwares are compiled by ezh-none-elf and PASS == exc_signal
# 0xCAFEBABE (slave: received 0xA5 and ACKed; master: address+data ACKed).
#===----------------------------------------------------------------------===#
set -e
cd "$(dirname "$0")"

BOARD_B_SERIAL="${BOARD_B_SERIAL:-GRA1CQLQ}"
BUILD=../../build/bin
OCD_CFG=../rt595-openocd.cfg

bringup() { # $1 = telnet port
python3 - "$1" <<'PY'
import socket, sys, time
port = int(sys.argv[1])
cmds = ["halt",
        "mww 0x40001040 0x40000000",   # CLKCTL0 PSCCTL0_SET: SMARTDMA clock
        "mww 0x40002634 0x00030000",   # SYSCTL0 PDRUNCFG1_CLR: power SMARTDMA SRAM
        "mww 0x4013500C 0x1",
        "mww 0x24100000 0xAA55AA55", "mdw 0x24100000"]
s = socket.create_connection(("127.0.0.1", port), timeout=5); s.settimeout(1.5)
out = b""; time.sleep(0.2)
for c in cmds:
    s.sendall((c+"\n").encode()); time.sleep(0.3)
    try:
        while True:
            d = s.recv(4096)
            if not d: break
            out += d
    except socket.timeout: pass
s.close()
sys.stdout.write(out.decode(errors="replace"))
PY
}

run_harness() { # $1 = gdb port, $2 = execute dir, $3 = logfile
    EZH_GDB_PORT="$1" EZH_TEST_EXECUTE_DIR="$(pwd)/$2" \
        "$BUILD/lldb" -b -o "command script import ezh_lldb_run_all.py" >"$3" 2>&1
}

read_word() { # $1 = telnet port, $2 = hex addr (0x...); echoes the 8-hex-digit value
python3 - "$1" "$2" <<'PY'
import socket, sys, time, re
port = int(sys.argv[1]); addr = sys.argv[2]
s = socket.create_connection(("127.0.0.1", port), timeout=5); s.settimeout(1.5)
out = b""; time.sleep(0.2)
for c in ["halt", "mdw " + addr]:
    s.sendall((c+"\n").encode()); time.sleep(0.3)
    try:
        while True:
            d = s.recv(4096)
            if not d: break
            out += d
    except socket.timeout: pass
s.close()
m = re.search(addr + r":\s*([0-9a-fA-F]+)", out.decode(errors="replace"))
print(m.group(1) if m else "????????")
PY
}

echo "=== 1. Build slave + master (bitslice interrupts off) ==="
rm -rf out && mkdir -p out
$BUILD/clang -target ezh-none-elf -mno-ezh-bitslice-interrupts -g -Os \
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror \
    -isystem ../../build/libc/libc/include -I ../../lldb/source/Plugins/Process/EZH/ \
    -D__TEST__ -DSTACK_SIZE_WORDS=1280 ../crt0.c -c -o out/crt0.o -fno-builtin
for t in ezh_i2c_slave ezh_i2c_master; do
    $BUILD/clang -target ezh-none-elf -mno-ezh-bitslice-interrupts -Os \
        -ffunction-sections -fdata-sections -Wall -Wextra -Werror \
        -isystem ../../build/libc/libc/include -I ../../libc -I . -I .. \
        $t.c -c -o out/$t.o
    $BUILD/ld.lld -T ../smartdma.ld --gc-sections out/crt0.o out/$t.o \
        ../../build/libc/libc/lib/libc_nano.a \
        ../../build/compiler-rt/lib/linux/libclang_rt.builtins-ezh.a \
        -Map=out/$t.map -o out/$t.elf
done
rm -rf out_slave out_master && mkdir -p out_slave out_master
cp out/ezh_i2c_slave.elf  out_slave/
cp out/ezh_i2c_master.elf out_master/
echo "    built out_slave/ezh_i2c_slave.elf, out_master/ezh_i2c_master.elf"

echo "=== 2. Start board B OpenOCD (:3334) on probe $BOARD_B_SERIAL ==="
EZH_ADAPTER_SERIAL="$BOARD_B_SERIAL" EZH_GDB_PORT=3334 EZH_TELNET_PORT=4445 \
    openocd -f "$OCD_CFG" >/tmp/ezh_ocd_boardB.log 2>&1 &
OCD_B_PID=$!
trap 'kill $OCD_B_PID 2>/dev/null || true' EXIT
sleep 2
if ! kill -0 $OCD_B_PID 2>/dev/null; then
    echo "ERROR: board B OpenOCD died — see /tmp/ezh_ocd_boardB.log"; cat /tmp/ezh_ocd_boardB.log; exit 1
fi

echo "=== 3. Bring up SMARTDMA on both boards ==="
echo "--- board A (:4444) ---"; bringup 4444 | tail -1
echo "--- board B (:4445) ---"; bringup 4445 | tail -1

echo "=== 4. Ignite SLAVE on board A (:3333); it waits for the master ==="
run_harness 3333 out_slave /tmp/ezh_slave_out.txt &
SLAVE_PID=$!

echo "    (giving the slave a head start to ignite and start polling the bus)"
sleep 7

echo "=== 5. Ignite MASTER on board B (:3334); it drives one write of 0xA5 ==="
run_harness 3334 out_master /tmp/ezh_master_out.txt
wait $SLAVE_PID 2>/dev/null || true

# The slave runs asynchronously and may complete just after its harness poll
# window closes, so read its result straight from board A's SRAM (the harness
# has detached by now, leaving the bus free).
SL_EXC=$($BUILD/llvm-readelf -s out/ezh_i2c_slave.elf | awk '/ exc_signal$/{print $2}')
SL_RX=$($BUILD/llvm-readelf  -s out/ezh_i2c_slave.elf | awk '/ i2c_rx_byte$/{print $2}')
SLAVE_EXC=$(read_word 4444 "0x$SL_EXC")
SLAVE_RX=$(read_word 4444 "0x$SL_RX")

echo ""
echo "=== RESULTS ==="
MASTER_LINE=$(grep -iE "PASSED|FAILED" /tmp/ezh_master_out.txt | head -1)
echo "--- MASTER (board B, :3334): $MASTER_LINE"
echo "--- SLAVE  (board A, :3333) direct read: exc_signal=0x$SLAVE_EXC  rx_byte=0x$SLAVE_RX"
if echo "$MASTER_LINE" | grep -qi PASSED && [ "$SLAVE_EXC" = "cafebabe" ]; then
    echo ""
    echo ">>> I2C LOOPBACK PASSED: master ACKed, slave received 0x$SLAVE_RX <<<"
else
    echo ""
    echo ">>> I2C LOOPBACK FAILED <<<"
fi
