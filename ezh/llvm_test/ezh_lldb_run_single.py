#!/usr/bin/env python3
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

import lldb
import sys
import os
import time
import signal
import hashlib

# Ensure we can import from the current directory
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ezh_common

# Restore default SIGINT handler immediately after importing lldb
signal.signal(signal.SIGINT, signal.default_int_handler)

def run_single_test(debugger):
    print("\n================================================")
    print("          EZH LLDB SINGLE-TEST RUNNER           ")
    print("================================================\n")

    # Target the local symlink ezh_test.elf established by the script runner!
    test_elf_path = "ezh_test.elf"

    if not os.path.exists(test_elf_path):
        # Fallback to checking if an argument was passed to the script
        if len(sys.argv) > 1 and os.path.exists(sys.argv[1]):
            test_elf_path = sys.argv[1]
        else:
            print(f"Error: Target ELF {test_elf_path} does not exist! Did you build the test?")
            return

    # Resolve the build directory dynamically (exactly matching Lit!)
    def get_build_dir(path):
        path = os.path.abspath(path)
        if os.path.isfile(path):
            path = os.path.dirname(path)
        if "SingleSource" in path:
            return path.split("SingleSource")[0]
        return path

    build_dir = get_build_dir(test_elf_path)
    if "SingleSource" not in build_dir:
        # Relocated path fallback (we are inside ezh/llvm_test/ now, so parent-parent is llvm-project/)
        build_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../build-test-suite-regress-Os"))

    # Print MD5 for verification
    print(f"Loading test ELF: {test_elf_path}")
    try:
        with open(test_elf_path, "rb") as f:
            raw_md5 = hashlib.md5(f.read()).hexdigest()
        print(f"Raw MD5 Checksum:  {raw_md5}")
    except Exception as e:
        print(f"Warning: Could not calculate MD5: {e}")

    dummy_elf = os.path.join(build_dir, "SingleSource/Regression/C/gcc-c-torture/execute/GCC-C-execute-alloca-1")
    if not os.path.exists(dummy_elf):
        print(f"Error: Base ELF {dummy_elf} does not exist! Please run CMake regression configuration first.")
        return

    # Call the shared JTAG engine in Standalone Mode!
    # We use the safe 500ms polling rate (0.5s) for CLI runs.
    stdout_str, exit_code, exc_val = ezh_common.run_ezh_core(
        debugger,
        test_elf_path,
        dummy_elf=dummy_elf,
        poll_interval=0.5,    # 500ms polling rate
        max_timeout=20.0
    )

    # Print results
    if exit_code == 0:
        print("================================================")
        print("   SUCCESS: SINGLE TEST PASSED 100% GREEN!      ")
        print(f"   Final exc_signal: 0x{exc_val:08x}")
        print("================================================\n")
        if stdout_str.strip():
            print("[Target Output]")
            print(stdout_str.strip())
            print("================================================")
    else:
        print("================================================")
        print(f"   FAILURE: Test stopped with signal: 0x{exc_val:08x}")
        print("================================================\n")

        if stdout_str.strip():
            print("[Target Output]")
            print(stdout_str.strip())
            print("================================================")

def __lldb_init_module(debugger, internal_dict):
    run_single_test(debugger)

if __name__ == "__main__":
    # Support standalone execution directly from the shell!
    lldb.SBDebugger.Initialize()
    dbg = lldb.SBDebugger.Create()
    try:
        run_single_test(dbg)
    finally:
        lldb.SBDebugger.Terminate()
