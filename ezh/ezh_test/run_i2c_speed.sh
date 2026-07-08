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
#
# Wrapper for ezh_i2c_lab.py: assumes board A's OpenOCD is already up on
# :3333/:4444; starts board B's on :3334/:4445, builds the common crt0, runs
# the lab (payload tracking + speed sweep + SCL-frequency measurement).
set -e
cd "$(dirname "$0")"
BUILD=../../build/bin

EZH_ADAPTER_SERIAL="${BOARD_B_SERIAL:-GRA1CQLQ}" EZH_GDB_PORT=3334 EZH_TELNET_PORT=4445 \
    openocd -f ../rt595-openocd.cfg >/tmp/ezh_ocd_boardB_speed.log 2>&1 &
OCD_B=$!
trap 'kill $OCD_B 2>/dev/null || true' EXIT
sleep 2
kill -0 $OCD_B 2>/dev/null || { echo "board B OpenOCD failed:"; cat /tmp/ezh_ocd_boardB_speed.log; exit 1; }

rm -rf out && mkdir -p out
$BUILD/clang -target ezh-none-elf -mno-ezh-bitslice-interrupts -g -Os \
    -ffunction-sections -fdata-sections -Wall -Wextra -Werror \
    -isystem ../../build/libc/libc/include -I ../../lldb/source/Plugins/Process/EZH/ \
    -D__TEST__ -DSTACK_SIZE_WORDS=1280 ../crt0.c -c -o out/crt0.o -fno-builtin

python3 ezh_i2c_lab.py
