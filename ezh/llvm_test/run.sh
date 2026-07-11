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
# EZH SingleSource Regression C Test Suite Runner

# This script configures the LLVM test-suite for ezh-none-elf, compiles the
# SingleSource Regression C tests, and runs them inside a single LLDB debugger
# session.
#
# Usage:
#   ./run.sh [options] [filter_regex]
#
# Options:
#   --skip-build                   Skip CMake configuration and compilation;
#                                  run existing binaries.
#   --disable-bitslice-interrupts  Pass -mno-ezh-bitslice-interrupts compiler
#                                  flag during build.
#   -O<level>                      Specify optimization level to build and run
#                                  (e.g., -O0, -O2, -Os). Can be passed multiple
#                                  times. Defaults to -O0 and -Os if omitted.
#   --filter=<regex>               Filter test execution using a regular
#                                  expression (can also be passed as a
#                                  standalone positional argument without the
#                                  --filter= prefix).
#
# Examples:
#   ./run.sh                                    # Build/run O0 & Os default
#   ./run.sh -O2 -O3                            # Build/run ONLY O2 and O3
#   ./run.sh --skip-build                       # Run without rebuilding
#   ./run.sh strncmp-1 -O2                      # Filter test matching at O2
#===----------------------------------------------------------------------===#
set -e

# Running from ezh/llvm-test/ folder:
EZH_TEST_DIR=$(pwd)
ROOT_DIR=${EZH_TEST_DIR}/../..
TEST_SUITE_DIR=${EZH_TEST_DIR}/llvm-test-suite
BUILD_DIR_O0=${ROOT_DIR}/build-test-suite-regress-O0
BUILD_DIR_OS=${ROOT_DIR}/build-test-suite-regress-Os

echo "=== Killing Stale Debugger Sessions ==="
killall -9 lldb 2>/dev/null || true

SKIP_BUILD=false
DISABLE_BITSLICE_INTERRUPTS=0
FILTER_ARG=""
OPT_LEVELS=()

for arg in "$@"; do
    if [ "$arg" == "--skip-build" ]; then
        SKIP_BUILD=true
    elif [ "$arg" == "--disable-bitslice-interrupts" ]; then
        DISABLE_BITSLICE_INTERRUPTS=1
    elif [[ "$arg" == --filter=* ]]; then
        FILTER_ARG="${arg#--filter=}"
    elif [[ "$arg" =~ ^-O[0-3szfast]+$ ]]; then
        OPT_LEVELS+=("${arg#-}")
    elif [ "$arg" != "--filter" ]; then
        FILTER_ARG="$arg"
    fi
done

if [ ${#OPT_LEVELS[@]} -eq 0 ]; then
    OPT_LEVELS=("O0" "Os")
fi

if [ "$SKIP_BUILD" = false ]; then
    BITSLICE_FLAGS=""
    if [ "$DISABLE_BITSLICE_INTERRUPTS" -eq 1 ]; then
        echo "=== Disabling bitslice interrupts ==="
        echo "    (-mno-ezh-bitslice-interrupts)"
        BITSLICE_FLAGS="-mno-ezh-bitslice-interrupts"
    fi

    echo "=== Compiling EZH Startup Code (crt0.o) ==="
    mkdir -p ${EZH_TEST_DIR}/out

    # Recompile crt0.o with correct complete cross-compiler includes/flags!
    ${ROOT_DIR}/build/bin/clang \
        -target ezh-none-elf \
        ${BITSLICE_FLAGS} \
        -g -O0 \
        -fdwarf-exceptions \
        -ffunction-sections \
        -fdata-sections \
        -Wall -Wextra -Werror \
        -isystem ${ROOT_DIR}/build/libc/libc/include \
        -I ${ROOT_DIR}/lldb/source/Plugins/Process/EZH/ \
        -D__TEST__ \
        -DSTACK_SIZE_WORDS=262144 \
        -DSTDOUT_BUF_SIZE=4096 \
        ${EZH_TEST_DIR}/../crt0.c \
        -c -o ${EZH_TEST_DIR}/out/crt0.o \
        -fno-builtin

    ${ROOT_DIR}/build/bin/clang \
        -target ezh-none-elf \
        ${BITSLICE_FLAGS} \
        -g -O0 \
        -ffunction-sections \
        -fdata-sections \
        -Wall -Wextra -Werror \
        -isystem ${ROOT_DIR}/build/libc/libc/include \
        -D__TEST__ \
        -DSTDOUT_BUF_SIZE=4096 \
        ${EZH_TEST_DIR}/../libc_stubs/libc_stubs.c \
        -c -o ${EZH_TEST_DIR}/out/libc_stubs.o \
        -fno-builtin

    echo "=== Temporarily Bypassing Unsupported Tests ==="
    # We bypass unsupported tests by moving them to a local gitignored
    # out/ folder before CMake configuration, and guarantee they are
    # restored instantly on exit!
    BYPASS_FILES=(
      # Requires real filesystem support (uses freopen/fopen):
      "SingleSource/Regression/C/gcc-c-torture/execute/fprintf-2.c"
      "SingleSource/Regression/C/gcc-c-torture/execute/user-printf.c"
      "SingleSource/Regression/C/gcc-c-torture/execute/printf-2.c"

      # %hhd isn't supported in LLVM
      "SingleSource/Regression/C/gcc-c-torture/execute/pr78622.c"

      # C++ STL header reliant tests (standard streams like fstream):
      "SingleSource/Regression/C++/ofstream_ctor.cpp"

      # Extremely slow loop validation sweeps:
      "SingleSource/Regression/C/uint64_to_float.c"
    )

    for FILE in "${BYPASS_FILES[@]}"; do
      SRC_PATH="${TEST_SUITE_DIR}/${FILE}"
      BASE_NAME=$(basename "${FILE}")
      BACKUP_PATH="${EZH_TEST_DIR}/out/${BASE_NAME}.bak"
      if [ -e "${SRC_PATH}" ]; then
        if [ -d "${SRC_PATH}" ]; then
          mv "${SRC_PATH}" "${EZH_TEST_DIR}/out/${BASE_NAME}_dir.bak"
        else
          mv "${SRC_PATH}" "${BACKUP_PATH}"
        fi
      fi
    done

    # Ensure we always restore the files on exit!
    trap '
    for FILE in "${BYPASS_FILES[@]}"; do
      SRC_PATH="${TEST_SUITE_DIR}/${FILE}"
      BASE_NAME=$(basename "${FILE}")
      BACKUP_PATH="${EZH_TEST_DIR}/out/${BASE_NAME}.bak"
      DIR_BACKUP_PATH="${EZH_TEST_DIR}/out/${BASE_NAME}_dir.bak"
      if [ -d "${DIR_BACKUP_PATH}" ]; then
        mv "${DIR_BACKUP_PATH}" "${SRC_PATH}"
      fi
      if [ -f "${BACKUP_PATH}" ]; then
        mv "${BACKUP_PATH}" "${SRC_PATH}"
      fi
    done
    echo "=== Restored all bypassed test cases ==="
    ' EXIT

    configure_and_build() {
        OPT=$1
        BDIR=$2
        echo "=== Configuring LLVM Test Suite via CMake for ${OPT} ==="
        rm -rf "${BDIR}"
        mkdir -p "${BDIR}"

        COMP_RT="${ROOT_DIR}/build/compiler-rt/lib/linux"
        RT_BUILTIN="${COMP_RT}/libclang_rt.builtins-ezh.a"
        LIBC_DIR="${ROOT_DIR}/build/libc/libc/lib"

        COMMON_FLAGS="-target ezh-none-elf ${BITSLICE_FLAGS} -nostdlibinc \
            -isystem ${EZH_TEST_DIR}/../libc_stubs \
            -I ${ROOT_DIR}/lldb/source/Plugins/Process/EZH/ \
            -D__TEST__ -DSTACK_SIZE_WORDS=262144 -DSTDOUT_BUF_SIZE=4096 \
            -${OPT} -ffunction-sections -fdata-sections \
            -DSIGNAL_SUPPRESS -Dalloca=__builtin_alloca"

        CFLAGS="${COMMON_FLAGS} \
            -isystem ${ROOT_DIR}/build/libc/libc/include"

        CXXFLAGS="${COMMON_FLAGS} -fexceptions \
            -isystem ${ROOT_DIR}/build/libc/include/c++/v1 \
            -isystem ${ROOT_DIR}/build/libc/libc/include \
            -D_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE"

        LDFLAGS="-L${ROOT_DIR}/build/libc/lib -L${LIBC_DIR} \
            --gc-sections --discard-locals \
            -T ${EZH_TEST_DIR}/smartdma_large.ld \
            ${EZH_TEST_DIR}/out/crt0.o ${EZH_TEST_DIR}/out/libc_stubs.o ${RT_BUILTIN} \
            ${LIBC_DIR}/libc.a ${LIBC_DIR}/libm.a \
            -lc++ -lc++abi -lunwind"

        SUBDIRS="SingleSource/Regression/C;SingleSource/Regression/C++"
        C_LINK="<CMAKE_LINKER> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS>"
        C_LINK="${C_LINK} <OBJECTS> -o <TARGET> <LINK_LIBRARIES>"
        CXX_LINK="<CMAKE_LINKER> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS>"
        CXX_LINK="${CXX_LINK} <OBJECTS> -o <TARGET> <LINK_LIBRARIES>"

        cmake -S "${TEST_SUITE_DIR}" -B "${BDIR}" \
            -DCMAKE_TOOLCHAIN_FILE="${EZH_TEST_DIR}/ezh_toolchain.cmake" \
            -DARCH=ARM \
            -DCMAKE_C_COMPILER="${ROOT_DIR}/build/bin/clang" \
            -DCMAKE_C_COMPILER_TARGET=ezh-none-elf \
            -DTEST_SUITE_USER_MODE_EMULATION=ON \
            -DCMAKE_C_FLAGS="${CFLAGS}" \
            -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
            -DTEST_SUITE_SUBDIRS="${SUBDIRS}" \
            -DCMAKE_LINKER="${ROOT_DIR}/build/bin/ld.lld" \
            -DCMAKE_C_LINK_EXECUTABLE="${C_LINK}" \
            -DCMAKE_CXX_LINK_EXECUTABLE="${CXX_LINK}" \
            -DCMAKE_EXE_LINKER_FLAGS="${LDFLAGS}" \
            -DCMAKE_BUILD_TYPE=Debug \
            -G Ninja

        echo "=== Compiling SingleSource Regression Tests for ${OPT} ==="
        ninja -C "${BDIR}"
    }

    for OPT in "${OPT_LEVELS[@]}"; do
        configure_and_build "${OPT}" \
            "${ROOT_DIR}/build-test-suite-regress-${OPT}"
    done
fi

echo "=== Running Tests (${OPT_LEVELS[*]} in single session) ==="
mkdir -p "${EZH_TEST_DIR}/out"
JSON_OUT="${EZH_TEST_DIR}/out/ezh_lit_results.json"

echo "Output JSON Log: ${JSON_OUT}"
echo "Running tests over JTAG sequentially... (Est: ~6-8 minutes)"

# Construct python list from arguments safely to forward to lit runner
PY_ARGS="['-o', '${JSON_OUT}']"
if [ -n "$FILTER_ARG" ]; then
    echo "=== Filtering tests matching: ${FILTER_ARG} ==="
    PY_ARGS="${PY_ARGS} + ['--filter', '${FILTER_ARG}']"
fi

for OPT in "${OPT_LEVELS[@]}"; do
    PY_ARGS="${PY_ARGS} + ['${ROOT_DIR}/build-test-suite-regress-${OPT}']"
done

# Execute LLDB lit runner inside LLDB python process
"${ROOT_DIR}/build/bin/lldb" -b \
    -o "script import ezh_lit_runner; ezh_lit_runner.main(${PY_ARGS})"

echo "=== Lit Run Completed Successfully! ==="
