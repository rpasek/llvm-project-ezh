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
# EZH Ctimer Test Runner
#===----------------------------------------------------------------------===#
set -e

echo "=============================================="
echo "       EZH Ctimer Test Runner                 "
echo "=============================================="

# 1. Build the test
echo "Building ctimer_test..."
make clean
make -j$(nproc)

# 2. Run on EZH Hardware via JTAG
echo "Running on EZH Hardware..."
../../build/bin/lldb -b -s ctimer_test_runner.lldb > out/ctimer_run.log 2>&1

# 3. Extract EZH result (exc_signal)
EZH_RESULT=$(grep -A 1 "p/x exc_signal" out/ctimer_run.log | tail -n 1 | \
    grep -o "0x[0-9a-fA-F]*" || true)

echo "EZH exc_signal:  $EZH_RESULT"

echo "----------------------------------------------"
if [ "$EZH_RESULT" == "0x00000005" ] || [ "$EZH_RESULT" == "0x5" ]; then
    echo " RESULT: PASSED!"
    echo "----------------------------------------------"
    exit 0
else
    echo " RESULT: FAILED! Expected 0x00000005, got '$EZH_RESULT'"
    echo "----------------------------------------------"
    # Print log for debugging if failed
    cat out/ctimer_run.log
    exit 1
fi
