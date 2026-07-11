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

# ===----------------------------------------------------------------------===#
# EZH LLVM-Lit Test Runner
# ===----------------------------------------------------------------------===#
import os
import re
import signal
import sys
import time
import lldb

_dir = os.path.dirname(__file__)
sys.path[:0] = [
    os.path.join(_dir, "llvm-test-suite"),
    os.path.join(_dir, "../../llvm/utils/lit"),
    os.path.join(_dir, ".."),
]
from ezh_launcher import run_test, connect

import lit.formats
import lit.Test
import litsupport.testplan
import litsupport.testfile
import litsupport.shellcommand
import lit.run
import lit.worker
import lit.TestingConfig
import lit.main

from functools import partial

# Restore default SIGINT handler immediately after importing lldb
# because lldb overrides it with its own C++ signal handlers.
signal.signal(signal.SIGINT, signal.default_int_handler)


def ezh_executeScript(
    debugger,
    target,
    process,
    original_executeScript,
    context,
    script,
    scriptBaseName,
    useExternalSh=True,
):
    if scriptBaseName != "run":
        # Run all other scripts (prepare, verify, metric) on host!
        return original_executeScript(
            context, script, scriptBaseName, useExternalSh
        )

    test_elf_path = context.executable
    if not test_elf_path or not os.path.exists(test_elf_path):
        print(f"\n[EZH Lit Error] Target ELF {test_elf_path} not found!")
        return "", "Target ELF not found", 1, None

    stdout_str = ""
    exitCode = 0
    err_msg = ""

    # Resolve stdin redirection from the Lit command line
    stdin_file = None
    if context.parsed_runscript:
        run_cmd_str = context.parsed_runscript[0]
        for c in run_cmd_str.split(";"):
            try:
                cmd = litsupport.shellcommand.parse(c.strip())
                if cmd.stdin is not None:
                    stdin_file = cmd.stdin
                    break
            except:
                pass

    # Add module to shared target
    module = target.AddModule(test_elf_path, None, None)
    if not module or not module.IsValid():
        print(
            f"Error: Failed to add ELF module to shared target: {test_elf_path}"
        )
        return

    stdin_data = None
    if stdin_file and os.path.exists(stdin_file):
        with open(stdin_file, "rb") as f_in:
            stdin_data = f_in.read()

    start_time = time.time()
    try:
        exc_val, stdout_str, launch_err = run_test(
            debugger,
            target,
            process,
            test_elf_path,
            timeout=20.0,
            stdin_data=stdin_data,
        )
    finally:
        if module and module.IsValid():
            target.RemoveModule(module)

    exec_time = time.time() - start_time
    exitCode = 0 if (launch_err is None and exc_val == 0xCAFEBABE) else 1
    if exitCode != 0:
        err_msg = f"Aborted on target with signal: 0x{exc_val:08x}"

    # Write stdout_str directly to the temporary output file %o!
    # This allows standard TestPlan VERIFY step to run on the host unmodified!
    outfile = context.tmpBase + ".out"
    try:
        os.makedirs(os.path.dirname(outfile), exist_ok=True)
        # Write output file
        with open(outfile, "w", encoding="utf-8") as out_f:
            out_f.write(stdout_str)

        # Write fake .time file to support litsupport.modules.timeit metric
        # collectors!
        timefile = context.tmpBase + ".time"
        with open(timefile, "w", encoding="utf-8") as time_f:
            time_f.write(f"user {exec_time:.6f}\n")
            time_f.write("sys 0.000000\n")
            time_f.write("real 0.000000\n")
            time_f.write("maxrss 0\n")
    except Exception as file_err:
        exitCode = 1
        err_msg = f"Failed to write target output/metrics: {file_err}"

    # Record execution trace in test output
    context.result_output += f"\n[Target Run: {test_elf_path}]\n"
    if exitCode != 0:
        context.result_output += f"Error: {err_msg}\n"

    return stdout_str, err_msg, exitCode, None


original_finish = lit.TestingConfig.TestingConfig.finish


def ezh_testing_config_finish(self, litConfig):
    original_finish(self, litConfig)
    if self.name == "test-suite":
        exec_root = getattr(self, "test_exec_root", "") or ""
        match = re.search(r"-O[0-3szfast]+", exec_root)
        if match:
            self.name = f"test-suite{match.group(0)}"


def ezh_run_execute(self, deadline):
    print(
        "[EZH Lit] Bypassing multiprocessing. Running tests sequentially in"
        " main thread..."
    )
    lit.worker.initialize(self.lit_config, {})

    for idx, test in enumerate(self.tests):
        if time.time() > deadline:
            print("[EZH Lit] Timeout reached!")
            break

        # Run sequentially in the main thread!
        remote_test = lit.worker.execute(test)

        self._update_test(self.tests[idx], remote_test)
        self.progress_callback(self.tests[idx])

        if remote_test.isFailure():
            self.failures += 1
            if self.failures == self.max_failures:
                break


def main(lit_args=None):
    if lit_args is None:
        lit_args = []

    # Find positional arguments (test paths or build dirs) that exist and are
    # not json logs
    positional_args = [
        arg
        for arg in lit_args
        if not arg.startswith("-")
        and os.path.exists(arg)
        and not arg.endswith(".json")
    ]

    if not positional_args:
        print(
            "Error: No test directories or paths provided to ezh_lit_runner.py!"
        )
        return

    build_dir = os.path.abspath(positional_args[0])

    print("\n==================================================")
    print("               EZH LLDB LIT RUNNER                  ")
    print("==================================================\n")

    # Initialize LLDB & Connect
    print("[EZH Lit] Initializing remote connection...")
    # Using GCC-C-execute-alloca-1 as the base target. We need to load an ELF
    # before connecting to inform LLDB what arch we intend to use.
    dummy_elf = os.path.join(
        build_dir,
        "SingleSource/Regression/C/gcc-c-torture/execute/"
        "GCC-C-execute-alloca-1",
    )
    if not os.path.exists(dummy_elf):
        print(
            f"Error: Base ELF {dummy_elf} does not exist! Please run CMake"
            " regression configuration first."
        )
        return

    debugger, target, process, conn_err = connect(dummy_elf)
    if conn_err:
        print(f"Error: {conn_err}")
        return

    # Save the original script executor and apply monkey-patches!
    original_executeScript = litsupport.testplan._executeScript
    litsupport.testplan._executeScript = partial(
        ezh_executeScript, debugger, target, process, original_executeScript
    )

    # Monkey-patch TestingConfig to rename the test suite based on build dir
    lit.TestingConfig.TestingConfig.finish = ezh_testing_config_finish

    # Monkey-patch lit.run.Run to bypass multiprocessing completely!
    lit.run.Run._execute = ezh_run_execute

    # ==================================================
    # 3. Execute Lit Main
    # ==================================================
    print("[EZH Lit] Monkey-patching complete! Run Lit Main...")

    # Set up arguments.
    # CRITICAL: We enforce "-j 1" to guarantee sequential execution over
    # single remote connection!
    sys.argv = ["lit", "-j", "1", "-v"]

    # Forward the lit arguments directly
    sys.argv.extend(lit_args)

    try:
        lit.main.main()
    except KeyboardInterrupt:
        print("\n[EZH Lit] Interrupted by user!")
    finally:
        print("\n[EZH Lit] Detaching from remote...")
        process.Detach()


if __name__ == "__main__":
    main(sys.argv[1:])
