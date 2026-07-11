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

import os
import re
import sys
import time
import traceback
import lldb

sys.path[:0] = [os.path.join(os.path.dirname(__file__), "..")]
from ezh_launcher import run_test, get_symbol, connect


class Tee:

    def __init__(self, *files):
        self.files = files

    def write(self, obj):
        for f in self.files:
            f.write(obj)
            f.flush()

    def flush(self):
        for f in self.files:
            f.flush()


def run_all_tests(debugger):
    print("\n================================================")
    print("              EZH LLDB TEST RUNNER              ")
    print("================================================\n")

    # Discover all compiled ELF test cases inside the build directory
    execute_dir = os.environ.get("EZH_TEST_EXECUTE_DIR")
    if not execute_dir or not os.path.exists(execute_dir):
        print(f"Error: Execute directory {execute_dir} does not exist!")
        return

    tests = [
        os.path.join(root, f)
        for root, _, files in os.walk(execute_dir)
        for f in files
        if f.endswith(".elf")
    ]

    if not tests:
        print(
            f"Error: No valid ELF test cases discovered inside {execute_dir}!"
        )
        return

    print(f"Discovered {len(tests)} tests.")

    # Connect remotely at startup using the first ELF to register
    # EZH architecture
    first_test = tests[0]
    print(f"Connecting to remote using base ELF: {first_test}")
    debugger, target, process, conn_err = connect(first_test)
    if conn_err:
        print(f"Error: {conn_err}")
        return

    interpreter = debugger.GetCommandInterpreter()
    res = lldb.SBCommandReturnObject()

    # Resolve active target process context
    if not process or not process.IsValid():
        print("Error: Failed to retrieve active process context!")
        return

    error = lldb.SBError()

    passed_count = 0
    failed_tests = []

    summary_path = os.path.join(
        os.path.dirname(__file__), "ezh_regression_summary.txt"
    )
    log_file = open(summary_path, "w")
    original_stdout = sys.stdout
    sys.stdout = Tee(original_stdout, log_file)

    print("================================================")
    print("                EZH TEST RESULTS                ")
    print("================================================")
    print(f" Discovered: {len(tests)} tests\n")

    try:
        for idx, test_path in enumerate(tests):
            test_name = os.path.basename(test_path)
            print(
                f"[{idx + 1}/{len(tests)}] Loading & executing {test_name}... ",
                end="",
                flush=True,
            )

            # Add the new test ELF as a module using Python SB API
            module = target.AddModule(test_path, None, None)
            if not module or not module.IsValid():
                raise Exception(f"Failed to import module: {test_path}")

            stdout_str = ""
            try:
                # Halt core, reset hardware, load ELF module, initialize
                # state, ignite core, and poll completion
                exc_val, stdout_str, launch_err = run_test(
                    debugger, target, process, test_path
                )
                if launch_err:
                    raise Exception(launch_err)
            except Exception as run_err:
                exc_val = 0xDEADC0DE
                print(f"[Execution Error] {run_err} ", end="")

            # Validate results - EVERY successful test case MUST exit with
            # exc_signal == 0xCAFEBABE
            passed = exc_val == 0xCAFEBABE

            status_str = "PASSED" if passed else "FAILED"
            print(f"{status_str} (exc_signal: 0x{exc_val:08x})")
            if (
                stdout_str
                and stdout_str.strip()
                and stdout_str.strip() != "exit 0"
            ):
                print(f"[stdout]:\n{stdout_str.strip()}")
            if passed:
                passed_count += 1
            else:
                failed_tests.append((test_name, exc_val))

            try:
                with open(summary_path, "a") as sf:
                    sf.write(
                        f"[{idx + 1}/{len(tests)}] [{status_str}] {test_name}"
                        f" (exc_signal: 0x{exc_val:08x})\n"
                    )
            except:
                pass

            # Remove the test ELF after all validation checks have completed
            # This ensures symbols remain valid during stdout_buffer resolution
            if module and module.IsValid():
                target.RemoveModule(module)
    except Exception as e:
        traceback.print_exc()
    finally:
        print("\n================================================")
        print("               RUN SUMMARY                      ")
        print("================================================")
        print(f" Discovered: {len(tests)} tests")
        print(f" Passed:     {passed_count}")
        print(f" Failed:     {len(failed_tests)}")
        if failed_tests:
            print("\nFailing tests:")
            for name, val in failed_tests:
                print(f"  * {name} (exc_signal: 0x{val:08x})")
        print("================================================")
        print(f"\n[Summary saved safely to: ezh_regression_summary.txt]")

        sys.stdout.flush()

        print("\nDetaching from remote...")
        try:
            process.Detach()
        except Exception as detach_err:
            print(f"[Warning] Detach failed: {detach_err}")

        sys.stdout = original_stdout
        log_file.close()

    # Terminate LLDB
    interpreter.HandleCommand("quit", res)


def __lldb_init_module(debugger, internal_dict):
    run_all_tests(debugger)
