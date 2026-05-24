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
# EZH LLVM Toolchain and Runtimes Build Orchestrator
#===----------------------------------------------------------------------===#
set -e

ROOT_DIR=$(pwd)

DISABLE_BITSLICE_INTERRUPTS=0
RUN_TEST_FLAGS=""
COMMON_C_FLAGS="-Os -ffunction-sections -fdata-sections -Wno-everything"

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --disable-bitslice-interrupts)
            DISABLE_BITSLICE_INTERRUPTS=1
            RUN_TEST_FLAGS="--disable-bitslice-interrupts"
            ;;
        *)
            echo "Unknown parameter passed: $1"
            exit 1
            ;;
    esac
    shift
done

if [ "$DISABLE_BITSLICE_INTERRUPTS" -eq 1 ]; then
    echo "=== Global: Disabling bitslice interrupts (-mno-ezh-bitslice-interrupts) ==="
    COMMON_C_FLAGS="${COMMON_C_FLAGS} -mno-ezh-bitslice-interrupts"
fi

echo "=== Configure LLVM ==="
cmake -G Ninja \
      -B build \
      -S llvm \
      -DCMAKE_BUILD_TYPE=Debug \
      -DLLVM_TARGETS_TO_BUILD="" \
      -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=EZH \
      -DLLVM_DEFAULT_TARGET_TRIPLE=ezh-none-elf \
      -DLLVM_ENABLE_PROJECTS="clang;lld;lldb" \
      -DLLVM_ENABLE_WERROR=OFF \
      -DLLDB_ENABLE_PYTHON=ON

echo "=== Build LLVM ==="
ninja -C build #llc clang

# Changes to llvm and clang will go undetected changes when building the
# following libraries so it's important that we cleanly rebuild them
echo "=== Build compiler-rt ==="
rm -rf build/compiler-rt
cmake -S compiler-rt/lib/builtins -B build/compiler-rt \
    -DCMAKE_C_COMPILER=${ROOT_DIR}/build/bin/clang \
    -DCMAKE_CXX_COMPILER=${ROOT_DIR}/build/bin/clang++ \
    -DCMAKE_ASM_COMPILER=${ROOT_DIR}/build/bin/clang \
    -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
    -DCOMPILER_RT_BAREMETAL_BUILD=ON \
    -DCMAKE_C_COMPILER_TARGET=ezh-none-elf \
    -DCMAKE_ASM_COMPILER_TARGET=ezh-none-elf \
    -DCMAKE_C_COMPILER_WORKS=ON \
    -DCMAKE_CXX_COMPILER_WORKS=ON \
    -DCMAKE_ASM_COMPILER_WORKS=ON \
    -DCMAKE_C_FLAGS="${COMMON_C_FLAGS}" \
    -DCMAKE_ASM_FLAGS="-Os -Wno-everything" \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -G Ninja
ninja -v -C build/compiler-rt

# Common CMake arguments for both full and nano builds
COMMON_CMAKE_ARGS=(
    -DCMAKE_C_COMPILER="${ROOT_DIR}/build/bin/clang"
    -DCMAKE_CXX_COMPILER="${ROOT_DIR}/build/bin/clang++"
    -DCMAKE_ASM_COMPILER="${ROOT_DIR}/build/bin/clang"
    -DLLVM_LIBC_FULL_BUILD=ON
    -DRUNTIMES_USE_LIBC=llvm-libc
    -DLIBC_TARGET_TRIPLE=ezh-none-elf
    -DLIBC_TARGET_OS=baremetal
    -DLIBC_TARGET_ARCHITECTURE=ezh
    -DCMAKE_SYSTEM_NAME=Generic
    -DCMAKE_SYSTEM_PROCESSOR=ezh
    -DCMAKE_C_COMPILER_WORKS=1
    -DCMAKE_CXX_COMPILER_WORKS=1
    -DCMAKE_ASM_COMPILER_WORKS=1
    -DLLVM_INCLUDE_TESTS=OFF
    -DLIBC_INCLUDE_TESTS=OFF
    -DCMAKE_C_COMPILER_TARGET=ezh-none-elf
    -DCMAKE_CXX_COMPILER_TARGET=ezh-none-elf
    -DCMAKE_ASM_COMPILER_TARGET=ezh-none-elf
    -DCMAKE_C_FLAGS="${COMMON_C_FLAGS} -nostdlibinc -isystem ${ROOT_DIR}/ezh/include_shim -isystem ${ROOT_DIR}/build/libc/libc/include"
    -DCMAKE_CXX_FLAGS="${COMMON_C_FLAGS} -fexceptions -frtti -D_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE -nostdlibinc -isystem ${ROOT_DIR}/ezh/include_shim -isystem ${ROOT_DIR}/build/libc/libc/include -I${ROOT_DIR}/libcxxabi/include"
    -DLIBCXX_ENABLE_SHARED:BOOL=OFF
    -DLIBCXX_ENABLE_EXCEPTIONS:BOOL=ON
    -DLIBCXX_ENABLE_RTTI:BOOL=ON
    -DLIBCXX_ENABLE_THREADS:BOOL=OFF
    -DLIBCXX_ENABLE_MONOTONIC_CLOCK:BOOL=OFF
    -DLIBCXX_ENABLE_FILESYSTEM:BOOL=OFF
    -DLIBCXX_ENABLE_LOCALIZATION:BOOL=ON
    -DLIBCXX_ENABLE_WIDE_CHARACTERS:BOOL=ON
    -DLIBCXX_ENABLE_RANDOM_DEVICE:BOOL=OFF
    -DLIBCXXABI_ENABLE_SHARED:BOOL=OFF
    -DLIBCXXABI_ENABLE_RTTI:BOOL=ON
    -DLLVM_ENABLE_RTTI:BOOL=ON
    -DLIBCXXABI_ENABLE_EXCEPTIONS:BOOL=ON
    -DLIBCXXABI_ENABLE_THREADS:BOOL=OFF
    -DLIBCXXABI_BAREMETAL:BOOL=ON
    -DLIBCXXABI_USE_LLVM_UNWINDER:BOOL=ON
    -DLIBUNWIND_ENABLE_SHARED:BOOL=OFF
    -DLIBUNWIND_ENABLE_STATIC:BOOL=ON
    -DLIBUNWIND_ENABLE_THREADS:BOOL=OFF
    -DLIBUNWIND_SHARED_OUTPUT_NAME="unwind-shared"
    -DLIBCXX_CXX_ABI=libcxxabi
    -DLIBCXX_ENABLE_STATIC_ABI_LIBRARY:BOOL=OFF
    -DLIBCXX_SHARED_OUTPUT_NAME="c++-shared"
    -DLIBCXXABI_SHARED_OUTPUT_NAME="c++abi-shared"
    -DCMAKE_BUILD_TYPE=MinSizeRel
    -G Ninja
)

# Build libc first to generate all standard C headers
echo "=== Build libc ==="
rm -rf build/libc
cmake -S runtimes -B build/libc "${COMMON_CMAKE_ARGS[@]}" \
    -DLLVM_ENABLE_RUNTIMES="libc" \
    -DLIBC_CONF_PRINTF_DISABLE_FLOAT=OFF \
    -DLIBC_CONF_SCANF_DISABLE_FLOAT=OFF
ninja -C build/libc

# Rebuild libc_nano inside build/libc_nano (disabling float printf and scanf to prevent 20KB+ code space bloat on 32KB targets)
echo "=== Build libc_nano ==="
rm -rf build/libc_nano
cmake -S runtimes -B build/libc_nano "${COMMON_CMAKE_ARGS[@]}" \
    -DLLVM_ENABLE_RUNTIMES="libc" \
    -DLIBC_CONF_PRINTF_DISABLE_FLOAT=ON \
    -DLIBC_CONF_SCANF_DISABLE_FLOAT=ON \
    -DLIBC_CONF_PRINTF_MODULAR=OFF
ninja -C build/libc_nano

# Copy libc_nano.a into the libc folder
cp build/libc_nano/libc/lib/libc.a build/libc/libc/lib/libc_nano.a

echo "=== Build libunwind, libcxx and libcxxabi ==="
cmake -S runtimes -B build/libc "${COMMON_CMAKE_ARGS[@]}" \
    -DLLVM_ENABLE_RUNTIMES="libunwind;libcxx;libcxxabi" \
    -DLIBC_CONF_PRINTF_DISABLE_FLOAT=OFF \
    -DLIBC_CONF_SCANF_DISABLE_FLOAT=OFF
ninja -C build/libc

# Run basic EZH tests on target
echo "=== Running Basic EZH Tests on Target ==="
cd ezh/ezh_test
/bin/bash ./run.sh ${RUN_TEST_FLAGS}
cd ${ROOT_DIR}
