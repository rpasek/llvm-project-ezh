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

MANUAL=false
for arg in "$@"; do
    if [ "$arg" = "--manual" ]; then
        MANUAL=true
    fi
done

# Purge and recreate local output directory cleanly
rm -rf out
mkdir -p out

# Compile and link into ezh_test.elf
echo "Building ezh_test.elf..."
../../build/bin/clang \
    -target ezh-none-elf \
    -fuse-ld=lld \
    -g -Os \
    -fdwarf-exceptions -ffunction-sections -fdata-sections \
    -Wall -Wextra -Werror -Wundef \
    -isystem ../libc_stubs \
    -isystem ../../build/libc/libc/include \
    -I ../../lldb/source/Plugins/Process/EZH/ -I . -I ../ezh_test \
    -D__TEST__ \
    -fno-builtin -nostdlib \
    -Wl,-L../../build/libc/lib \
    -Wl,-L../../build/libc/libc/lib \
    -Wl,-T,../llvm_test/smartdma_large.ld \
    -Wl,--gc-sections,--discard-locals \
    -Wl,-Map=out/ezh_test.map \
    ../crt0.c ../libc_stubs/libc_stubs.c ezh_debug_test.c \
    ../../build/compiler-rt/lib/linux/libclang_rt.builtins-ezh.a \
    ../../build/libc/libc/lib/libc.a \
    ../../build/libc/libc/lib/libm.a \
    -lc++ -lc++abi -lunwind \
    -o out/ezh_test.elf

# Generate Disassembly
echo "Generating disassembly to out/ezh_test.disasm..."
../../build/bin/llvm-objdump \
    -d --triple=ezh out/ezh_test.elf \
    > out/ezh_test.disasm

if [ "$MANUAL" = true ]; then
    # 5. Launch LLDB stepping, unwinding, and stack frame repair session!
    echo "Launching LLDB synchronized debugging session..."
    killall -9 lldb || true
    ../../build/bin/lldb -o "command source -e 0 debug_test_runner.lldb"
else
    # Launch automated debug tests
    echo "Running debug verification suite..."
    python3 debug_test_runner.py
fi
