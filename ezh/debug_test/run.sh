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

# Create output directory
mkdir -p out

# 1. Recompile clean common objects (crt0.c)
echo "Building common objects..."
rm -f out/crt0.o
../../build/bin/clang -target ezh-none-elf -g -Os -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wundef -isystem ../../build/libc/libc/include -I ../../lldb/source/Plugins/Process/EZH/ -D__TEST__ -DSTACK_SIZE_WORDS=1280 ../crt0.c -c -o out/crt0.o -fno-builtin

# 2. Recompile ezh_debug_test.c
echo "Building ezh_debug_test..."
rm -f out/ezh_debug_test.o out/ezh_debug_test.elf
../../build/bin/clang -target ezh-none-elf -g -Os -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wundef -isystem ../../build/libc/libc/include -I . -I ../ezh_test -D__TEST__ ezh_debug_test.c -c -o out/ezh_debug_test.o -fno-builtin

# 3. Link into ezh_debug_test.elf
../../build/bin/ld.lld -T ../smartdma.ld --gc-sections --discard-locals out/crt0.o out/ezh_debug_test.o ../../build/libc/libc/lib/libc_nano.a ../../build/compiler-rt/lib/linux/libclang_rt.builtins-ezh.a -Map=out/ezh_debug_test.map -o out/ezh_debug_test.elf

# 4. Generate Disassembly
echo "Generating disassembly to out/ezh_debug_test.disasm..."
../../build/bin/llvm-objdump -d --triple=ezh out/ezh_debug_test.elf > out/ezh_debug_test.disasm

# 6. Launch native LLDB stepping, unwinding, and stack frame repair session!
echo "Launching native LLDB synchronized debugging session..."
killall -9 lldb lldb-server gdb-multiarch gdb || true
../../build/bin/lldb -o "command source -e 0 test_debug_ezh.lldb"
