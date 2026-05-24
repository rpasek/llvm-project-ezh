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
# EZH Comprehensive Debugger Verification Suite Launcher
#===----------------------------------------------------------------------===#
set -e

# Ensure we are in the directory of this script
CDPATH="" cd -- "$(dirname -- "$0")"

# Execute the interactive debugger tests
python3 run_comprehensive_debug_tests.py
