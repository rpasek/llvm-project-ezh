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
# EZH Accelerated JTAG Core Validation Suite Launcher
#===----------------------------------------------------------------------===#
set -e

# Purge and recreate local output directory cleanly
rm -rf out && mkdir -p out

DISABLE_BITSLICE_INTERRUPTS=0
TESTS=()

for arg in "$@"; do
    if [ "$arg" == "--disable-bitslice-interrupts" ]; then
        DISABLE_BITSLICE_INTERRUPTS=1
    else
        TESTS+=("$arg")
    fi
done

if [ ${#TESTS[@]} -eq 0 ]; then
    TESTS=(ezh_basic ezh_comp ezh_comp_64 ezh_comp_float ezh_comp_double ezh_arith ezh_other ezh_shift ezh_bit ezh_array ezh_arith_float ezh_arith_double ezh_print ezh_bitslice_test ezh_eh ezh_setjmp)
fi

BITSLICE_FLAGS=""
if [ "$DISABLE_BITSLICE_INTERRUPTS" -eq 1 ]; then
    echo "=== Disabling bitslice interrupts (-mno-ezh-bitslice-interrupts) ==="
    BITSLICE_FLAGS="-mno-ezh-bitslice-interrupts"
fi

# Build common objects once
echo "Building common objects..."
rm -f out/crt0.o
../../build/bin/clang -target ezh-none-elf ${BITSLICE_FLAGS} -g -Os -ffunction-sections -fdata-sections -Wall -Wextra -Werror -isystem ../../build/libc/libc/include -I ../../lldb/source/Plugins/Process/EZH/ -D__TEST__ -DSTACK_SIZE_WORDS=1280 ../crt0.c -c -o out/crt0.o -fno-builtin

# Pre-compile and Link all tests
for test_name in "${TESTS[@]}"; do
    echo "Compiling $test_name..."
    rm -f out/${test_name}.o out/${test_name}.elf
    if [ -f ${test_name}.cpp ]; then
        ../../build/bin/clang++ -target ezh-none-elf ${BITSLICE_FLAGS} -Os -fexceptions -ffunction-sections -fdata-sections -Wall -Wextra -Werror -isystem ../../build/libc/libc/include -I ../../libc -I . -I .. ${test_name}.cpp -c -o out/${test_name}.o
        ../../build/bin/ld.lld -T ../llvm_test/smartdma_large.ld --gc-sections --discard-locals out/crt0.o out/${test_name}.o -L../../build/libc/lib -L../../build/libc/libc/lib ../../build/compiler-rt/lib/linux/libclang_rt.builtins-ezh.a ../../build/libc/libc/lib/libc_nano.a ../../build/libc/libc/lib/libm.a -lc++ -lc++abi -lunwind -Map=out/${test_name}.map -o out/${test_name}.elf
    else
        ../../build/bin/clang -target ezh-none-elf ${BITSLICE_FLAGS} -Os -ffunction-sections -fdata-sections -Wall -Wextra -Werror -isystem ../../build/libc/libc/include -I ../../libc -I /usr/include/csmith -I . -I .. -DCSMITH_MINIMAL ${test_name}.c -c -o out/${test_name}.o
        ../../build/bin/ld.lld -T ../smartdma.ld --gc-sections --discard-locals out/crt0.o out/${test_name}.o ../../build/libc/libc/lib/libc_nano.a ../../build/compiler-rt/lib/linux/libclang_rt.builtins-ezh.a -Map=out/${test_name}.map -o out/${test_name}.elf
    fi

    # Generate Disassembly
    ../../build/bin/llvm-objdump -d --triple=ezh out/${test_name}.elf > out/${test_name}.disasm
done

# Dynamically flash and execute all tests over a SINGLE accelerated JTAG connection!
echo "=== Running all tests natively over JTAG ==="
export EZH_TEST_EXECUTE_DIR="$(pwd)/out"
../../build/bin/lldb -b -o "command script import ezh_lldb_run_all.py"
