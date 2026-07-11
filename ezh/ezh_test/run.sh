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

DISABLE_BITSLICE_INTERRUPTS=0
TESTS=()

for arg in "$@"; do
    if [ "$arg" == "--disable-bitslice-interrupts" ]; then
        DISABLE_BITSLICE_INTERRUPTS=1
    else
        TESTS+=("$arg")
    fi
done

BITSLICE_FLAGS=""
if [ "$DISABLE_BITSLICE_INTERRUPTS" -eq 1 ]; then
    echo "=== Disabling bitslice interrupts (-mno-ezh-bitslice-interrupts) ==="
    BITSLICE_FLAGS="-mno-ezh-bitslice-interrupts"
fi

MAKE_ARGS=("BITSLICE_FLAGS=${BITSLICE_FLAGS}")
if [ ${#TESTS[@]} -gt 0 ]; then
    MAKE_ARGS+=("ALL_TESTS=${TESTS[*]}")
fi

make clean
exec make -j$(nproc) "${MAKE_ARGS[@]}" run
