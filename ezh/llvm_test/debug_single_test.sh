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
# Isolated EZH Single-Test Compilation and JTAG Runner
# Usage: ./debug_single_test.sh <relative_path_to_test.c>
# Example: ./debug_single_test.sh llvm_test/llvm-test-suite/SingleSource/Regression/C/uint64_to_float.c
#===----------------------------------------------------------------------===#

set -e

DISABLE_BITSLICE_INTERRUPTS=0
OPT_LEVEL="-O0"
TEST_FILE=""
EXTRA_FLAGS=""

for arg in "$@"; do
    if [ "$arg" == "--disable-bitslice-interrupts" ]; then
        DISABLE_BITSLICE_INTERRUPTS=1
    elif [[ "$arg" =~ ^-O[0-9szZ]$ ]]; then
        OPT_LEVEL="$arg"
    elif [[ "$arg" == *.c || "$arg" == *.cpp ]]; then
        TEST_FILE="$arg"
    else
        EXTRA_FLAGS="${EXTRA_FLAGS} $arg"
    fi
done

if [ -z "${TEST_FILE}" ]; then
  echo "Error: Please specify the path to the test file."
  echo "Usage: $0 [--disable-bitslice-interrupts] [-O<level>] [extra_clang_flags...] <path_to_test.c>"
  exit 1
fi

TEST_NAME=$(basename "${TEST_FILE}" .c)

LLVM_TEST_DIR=$(pwd)
EZH_DIR=${LLVM_TEST_DIR}/..
ROOT_DIR=${EZH_DIR}/..
BUILD_DIR=${ROOT_DIR}/build-test-suite-regress

BITSLICE_FLAGS=""
if [ "$DISABLE_BITSLICE_INTERRUPTS" -eq 1 ]; then
  echo "=== Disabling bitslice interrupts (-mno-ezh-bitslice-interrupts) ==="
  BITSLICE_FLAGS="-mno-ezh-bitslice-interrupts"
fi

echo "=== 1. Compiling EZH Startup Code (crt0.o) ==="
mkdir -p ${LLVM_TEST_DIR}/out
${ROOT_DIR}/build/bin/clang -target ezh-none-elf ${BITSLICE_FLAGS} -g ${OPT_LEVEL} -ffunction-sections -fdata-sections -Wall -Wextra -Werror -isystem ${ROOT_DIR}/build/libc/libc/include -I ${ROOT_DIR}/lldb/source/Plugins/Process/EZH/ -D__TEST__ -DSTACK_SIZE_WORDS=262144 ${EZH_DIR}/crt0.c -c -o ${LLVM_TEST_DIR}/out/crt0.o -fno-builtin

echo "=== 2. Compiling Target Test: ${TEST_NAME} ==="
${ROOT_DIR}/build/bin/clang -target ezh-none-elf ${BITSLICE_FLAGS} -nostdlibinc -isystem ${LLVM_TEST_DIR}/include_shim -isystem ${ROOT_DIR}/build/libc/include/c++/v1 -isystem ${ROOT_DIR}/build/libc/libc/include -I ${ROOT_DIR}/lldb/source/Plugins/Process/EZH/ -I ${EZH_DIR} -I ${EZH_DIR}/ezh_test -D__TEST__ -DSTACK_SIZE_WORDS=262144 ${OPT_LEVEL} ${EXTRA_FLAGS} -ffunction-sections -fdata-sections -DSIGNAL_SUPPRESS -Dalloca=__builtin_alloca -Wno-implicit-function-declaration -Wno-int-conversion -Wno-implicit-int -Wno-incompatible-pointer-types -w -D_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE -g \
    "${TEST_FILE}" -c -o "${LLVM_TEST_DIR}/out/${TEST_NAME}.o"

echo "=== 3. Linking Target Executable ==="
${ROOT_DIR}/build/bin/ld.lld -L${ROOT_DIR}/build/libc/lib -L${ROOT_DIR}/build/libc/libc/lib --gc-sections --discard-locals -T smartdma_large.ld \
    "${LLVM_TEST_DIR}/out/${TEST_NAME}.o" \
    ${LLVM_TEST_DIR}/out/crt0.o \
    ${ROOT_DIR}/build/compiler-rt/lib/linux/libclang_rt.builtins-ezh.a \
    ${ROOT_DIR}/build/libc/libc/lib/libc.a \
    ${ROOT_DIR}/build/libc/libc/lib/libm.a \
    -lc++ \
    -lc++abi \
    -lunwind \
    -o "${LLVM_TEST_DIR}/out/ezh_test.elf"

echo "=== 4. Creating Local Symlink for test_ezh.lldb ==="
# test_ezh.lldb references target modules load ezh_test.elf
# We keep a temporary local symlink to make sure it resolves correctly!
rm -f "${LLVM_TEST_DIR}/ezh_test.elf"
ln -s "out/ezh_test.elf" "${LLVM_TEST_DIR}/ezh_test.elf"

echo "=== 5. Running Isolated Test Natively inside LLDB over JTAG remote ==="
${ROOT_DIR}/build/bin/lldb -b -o "script import ezh_lldb_run_single; ezh_lldb_run_single.run_single_test(lldb.debugger)"

echo "=== Finished Execution ==="
