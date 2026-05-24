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

set -e

SCRATCH_DIR="out"
OUT_DIR="out"
TEST_NAME="csmith_tiny"

# Purge and recreate local output directory cleanly
rm -rf out && mkdir -p out


echo "=============================================="
echo "       EZH Csmith Differential Test           "
echo "=============================================="

GENERATE_NEW=false
if [ "$1" == "--new" ]; then
    GENERATE_NEW=true
fi

if [ "$GENERATE_NEW" = true ]; then
    # 1. Generate random Csmith test
    echo "Generating new random test..."
    csmith --max-funcs 5 --max-expr-complexity 3 --max-block-size 3 --max-block-depth 3 --no-argc > ${SCRATCH_DIR}/csmith_generated.c
    # Copy to ezh_test
    cp ${SCRATCH_DIR}/csmith_generated.c ${TEST_NAME}.c
else
    if [ ! -f "${TEST_NAME}.c" ]; then
        echo "Error: Existing test ${TEST_NAME}.c not found! Use --new to generate one."
        exit 1
    fi
    echo "Using existing test ${TEST_NAME}.c..."
    cp ${TEST_NAME}.c ${SCRATCH_DIR}/csmith_generated.c
fi

# 2. Compile and run on Host (GCC)
echo "Running on host (GCC)..."
gcc -I /usr/include/csmith -I . -DCSMITH_MINIMAL ${SCRATCH_DIR}/csmith_generated.c -o ${SCRATCH_DIR}/csmith_host
HOST_OUT=$(${SCRATCH_DIR}/csmith_host)
HOST_CHECKSUM=$(echo "$HOST_OUT" | grep "checksum =" | cut -d' ' -f3 | tr -d '\r')
echo "Host Checksum: $HOST_CHECKSUM"

# 4. Compile for EZH Target
echo "Compiling for EZH..."
rm -f out/${TEST_NAME}.o out/${TEST_NAME}.elf out/crt0.o
../../build/bin/clang -target ezh-none-elf -g -Os -ffunction-sections -fdata-sections -Wno-everything -isystem ../../build/libc/libc/include -I ../../lldb/source/Plugins/Process/EZH/ -D__TEST__ ../crt0.c -c -o out/crt0.o -fno-builtin
../../build/bin/clang -target ezh-none-elf -g -Os -ffunction-sections -fdata-sections -Wno-everything -isystem ../../build/libc/libc/include -I /usr/include/csmith -I . -DCSMITH_MINIMAL ${TEST_NAME}.c -c -o out/${TEST_NAME}.o -fno-builtin
../../build/bin/ld.lld -T ../smartdma.ld --gc-sections --discard-locals out/crt0.o out/${TEST_NAME}.o ../../build/libc/libc/lib/libc_nano.a ../../build/compiler-rt/lib/linux/libclang_rt.builtins-ezh.a -Map=out/${TEST_NAME}.map -o out/${TEST_NAME}.elf
../../build/bin/llvm-objdump -d --triple=ezh out/${TEST_NAME}.elf > out/${TEST_NAME}.disasm

# 5. Run on EZH Hardware via JTAG
echo "Running on EZH Hardware..."
cp out/${TEST_NAME}.elf out/ezh_test.elf

# Capture LLDB output to log purely natively over JTAG!
../../build/bin/lldb -b -s ../test_ezh.lldb out/ezh_test.elf > out/csmith_run.log 2>&1

# 6. Extract EZH Checksum robustly from printf_buffer
EZH_CHECKSUM=$(grep -o "checksum = [0-9a-fA-F]*" out/csmith_run.log | head -n 1 | cut -d' ' -f3)

echo "EZH Checksum:  $EZH_CHECKSUM"

# 7. Check results
echo "----------------------------------------------"
if [ "$HOST_CHECKSUM" == "$EZH_CHECKSUM" ] && [ ! -z "$EZH_CHECKSUM" ]; then
    echo " RESULT: PASSED! Checksums match."
    echo "----------------------------------------------"
    exit 0
else
    echo " RESULT: FAILED! Checksums mismatch or empty."
    echo "----------------------------------------------"
    exit 1
fi
