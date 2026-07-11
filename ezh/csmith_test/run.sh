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
LLVM_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="${LLVM_DIR}/build"
EZH_DIR="${LLVM_DIR}/ezh"
CSMITH_TEST_DIR="${EZH_DIR}/csmith_test"
TOOLCHAIN_BIN_DIR="${BUILD_DIR}/bin"

mkdir -p out

echo "=============================================="
echo "       EZH Csmith Differential Test           "
echo "=============================================="

GENERATE_NEW=false
if [ "$1" == "--new" ] || [ ! -f "out/${TEST_NAME}.c" ]; then
    GENERATE_NEW=true
fi

if [ "$GENERATE_NEW" = true ]; then
    echo "Generating new random test (out/${TEST_NAME}.c)..."
    csmith --max-funcs 5 --max-expr-complexity 3 --max-block-size 3 \
        --max-block-depth 3 --no-argc > out/${TEST_NAME}.c
else
    echo "Using existing test out/${TEST_NAME}.c..."
fi

# Compile and run on Host (GCC)
echo "Running on host (GCC)..."
gcc -I /usr/include/csmith -I . -DCSMITH_MINIMAL \
    out/${TEST_NAME}.c -o out/csmith_host
HOST_OUT=$(./out/csmith_host)
HOST_CHECKSUM=$(echo "$HOST_OUT" | grep "checksum =" | cut -d' ' -f3 | \
    tr -d '\r')
echo "Host Checksum: $HOST_CHECKSUM"

# Compile for EZH Target
echo "Compiling for EZH..."
rm -f out/${TEST_NAME}.o out/${TEST_NAME}.elf out/crt0.o
(cd out && ${TOOLCHAIN_BIN_DIR}/clang \
    -target ezh-none-elf \
    -fuse-ld=lld \
    -g \
    -Os \
    -fdwarf-exceptions \
    -ffunction-sections \
    -fdata-sections \
    -Wno-everything \
    -isystem ${EZH_DIR}/libc_stubs \
    -isystem ${BUILD_DIR}/libc/libc/include \
    -I ${LLVM_DIR}/lldb/source/Plugins/Process/EZH/ \
    -I /usr/include/csmith \
    -I ${CSMITH_TEST_DIR} \
    -D__TEST__ \
    -DCSMITH_MINIMAL \
    -fno-builtin \
    -nostdlib \
    -Wl,-T,${EZH_DIR}/smartdma.ld,--gc-sections,--discard-locals \
    -Wl,-Map=${TEST_NAME}.map \
    ${EZH_DIR}/crt0.c \
    ${EZH_DIR}/libc_stubs/libc_stubs.c \
    ${CSMITH_TEST_DIR}/out/${TEST_NAME}.c \
    ${BUILD_DIR}/libc/libc/lib/libc_nano.a \
    ${BUILD_DIR}/compiler-rt/lib/linux/libclang_rt.builtins-ezh.a \
    -o ${TEST_NAME}.elf)

${TOOLCHAIN_BIN_DIR}/llvm-objdump -d --triple=ezh out/${TEST_NAME}.elf \
    > out/${TEST_NAME}.disasm

echo "Running on EZH Hardware..."
cp out/${TEST_NAME}.elf out/ezh_test.elf

${TOOLCHAIN_BIN_DIR}/lldb -b -s csmith_test_runner.lldb \
    > out/csmith_run.log 2>&1

# Extract EZH Checksum from printf_buffer
EZH_CHECKSUM=$(grep -o "checksum = [0-9a-fA-F]*" out/csmith_run.log | \
    head -n 1 | cut -d' ' -f3)

echo "EZH Checksum:  $EZH_CHECKSUM"

# Check results
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
