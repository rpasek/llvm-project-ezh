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

#===----------------------------------------------------------------------===#
# EZH Single-Process JTAG-Accelerated LLVM-Lit Test Runner (Unified TestPlan)
#===----------------------------------------------------------------------===#
import lldb
import sys
import os
import time
import signal
import ezh_common

# Restore default SIGINT handler immediately after importing lldb
# because lldb overrides it with its own C++ signal handlers.
signal.signal(signal.SIGINT, signal.default_int_handler)

def run_ezh_lit(lit_args=None):
    if lit_args is None:
        lit_args = []

    import argparse
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--build-dir', default=None)
    parsed, remaining_args = parser.parse_known_args(lit_args)

    # Find positional arguments (test paths) that exist and are not json logs
    positional_args = [
        arg for arg in remaining_args
        if not arg.startswith('-') and os.path.exists(arg) and not arg.endswith('.json')
    ]

    def get_build_dir(path):
        path = os.path.abspath(path)
        if os.path.isfile(path):
            path = os.path.dirname(path)
        if "SingleSource" in path:
            return path.split("SingleSource")[0]
        return path

    if parsed.build_dir:
        build_dir = os.path.abspath(parsed.build_dir)
    elif positional_args:
        build_dir = get_build_dir(positional_args[0])
    else:
        build_dir = os.path.abspath('../../build-test-suite-regress')

    print("\n==================================================")
    print("               EZH LLDB LIT RUNNER                ")
    print("==================================================\n")

    # 1. Initialize LLDB & Connect JTAG Remote ONCE at startup!
    print("[EZH Lit] Initializing JTAG remote connection...")
    debugger = lldb.SBDebugger.Create()

    # Using GCC-C-execute-alloca-1 as the base target.
    dummy_elf = os.path.join(build_dir, "SingleSource/Regression/C/gcc-c-torture/execute/GCC-C-execute-alloca-1")
    if not os.path.exists(dummy_elf):
        print(f"Error: Base ELF {dummy_elf} does not exist! Please run CMake regression configuration first.")
        return

    target = debugger.CreateTarget(dummy_elf)
    if not target or not target.IsValid():
        print("Error: Failed to create base target!")
        return

    error = lldb.SBError()
    process = target.ConnectRemote(debugger.GetListener(), "connect://localhost:3333", "ezh-remote", error)
    if not error.Success() or not process.IsValid():
        print(f"Error: JTAG connection failed: {error.GetCString()}")
        return

    # Disable memory cache to prevent stale reads of exc_signal
    res = lldb.SBCommandReturnObject()
    debugger.GetCommandInterpreter().HandleCommand('settings set target.process.disable-memory-cache true', res)

    # Tell LLDB not to intercept SIGINT (Ctrl-C) so it propagates natively to Python
    debugger.GetCommandInterpreter().HandleCommand('process handle SIGINT -s false -p false -n false', res)

    # Force initial JTAG Halt
    process.WriteMemory(0x40027024, (0xC0DE0000).to_bytes(4, 'little'), error)

    # ==================================================
    # 2. Import Lit & Monkey-Patch TestPlan Executor
    # ==================================================
    # Add paths to sys.path so we can import them
    lit_path = os.path.abspath("../../llvm/utils/lit")
    sys.path.insert(0, lit_path)
    sys.path.insert(0, os.path.abspath("llvm-test-suite"))

    import lit.formats
    import lit.Test
    import litsupport.testplan
    import litsupport.testfile
    import lit.run
    import lit.worker

    # Save the original script executor
    original_executeScript = litsupport.testplan._executeScript

    # Custom EZH JTAG Script Executor
    def ezh_executeScript(context, script, scriptBaseName, useExternalSh=True):
        if scriptBaseName == "run":
            test_elf_path = context.executable
            if not test_elf_path or not os.path.exists(test_elf_path):
                print(f"\n[EZH Lit Error] Target ELF {test_elf_path} not found!")
                return "", "Target ELF not found", 1, None

            is_base = (os.path.abspath(test_elf_path) == os.path.abspath(dummy_elf))

            stdout_str = ""
            exitCode = 0
            err_msg = ""

            # Resolve stdin redirection from the Lit command line
            from litsupport import shellcommand
            stdin_file = None
            if context.parsed_runscript:
                run_cmd_str = context.parsed_runscript[0]
                for c in run_cmd_str.split(';'):
                    try:
                        cmd = shellcommand.parse(c.strip())
                        if cmd.stdin is not None:
                            stdin_file = cmd.stdin
                            break
                    except:
                        pass

            # Call the shared JTAG engine in Shared/Lit Mode!
            # We pass the global target and process to reuse the connection,
            # the dummy_elf, and the resolved stdin_file.
            # We use the standard 50ms polling rate (0.05s) for Lit.
            start_time = time.time()
            stdout_str, exitCode, exc_val = ezh_common.run_ezh_core(
                debugger,
                test_elf_path,
                dummy_elf=dummy_elf,
                poll_interval=0.05,
                max_timeout=20.0,
                target=target,
                process=process,
                stdin_file=stdin_file
            )

            jtag_time = time.time() - start_time
            if exitCode != 0:
                # Get the hardware PC for error reporting (the engine already halted it)
                error = lldb.SBError()
                pc_bytes = process.ReadMemory(0x40027020, 4, error)
                pc_val = int.from_bytes(pc_bytes, 'little') if error.Success() else 0
                err_msg = f"Aborted on JTAG with signal: 0x{exc_val:08x} (Hardware PC: 0x{pc_val:08x})"

            # H. Write stdout_str directly to the temporary output file %o!
            # This allows standard TestPlan VERIFY step to run on the host unmodified!
            outfile = context.tmpBase + ".out"
            try:
                os.makedirs(os.path.dirname(outfile), exist_ok=True)
                # Write output file
                with open(outfile, "w", encoding="utf-8") as out_f:
                    out_f.write(stdout_str)

                # Write fake .time file to support litsupport.modules.timeit metric collectors!
                timefile = context.tmpBase + ".time"
                with open(timefile, "w", encoding="utf-8") as time_f:
                    time_f.write(f"user {jtag_time:.6f}\n")
                    time_f.write("sys 0.000000\n")
                    time_f.write("real 0.000000\n")
                    time_f.write("maxrss 0\n")
            except Exception as file_err:
                exitCode = 1
                err_msg = f"Failed to write JTAG output/metrics: {file_err}"

            # Record execution trace in test output
            context.result_output += f"\n[JTAG Run: {test_elf_path}]\n"
            if exitCode != 0:
                context.result_output += f"Error: {err_msg}\n"

            return stdout_str, err_msg, exitCode, None

        else:
            # Run all other scripts (prepare, verify, metric) natively on host!
            return original_executeScript(context, script, scriptBaseName, useExternalSh)

    # Apply monkey-patches!
    litsupport.testplan._executeScript = ezh_executeScript

    # Monkey-patch TestingConfig to dynamically rename the test suite based on build dir
    import lit.TestingConfig
    original_finish = lit.TestingConfig.TestingConfig.finish
    def ezh_testing_config_finish(self, litConfig):
        original_finish(self, litConfig)
        if self.name == "test-suite":
            exec_root = getattr(self, 'test_exec_root', '') or ''
            if '-O0' in exec_root:
                self.name = "test-suite-O0"
            elif '-Os' in exec_root:
                self.name = "test-suite-Os"
            elif '-O2nb' in exec_root:
                self.name = "test-suite-O2nb"
    lit.TestingConfig.TestingConfig.finish = ezh_testing_config_finish

    # Monkey-patch lit.run.Run to bypass multiprocessing completely!
    def ezh_run_execute(self, deadline):
        print("[EZH Lit] Bypassing multiprocessing. Running tests sequentially in main thread...")
        lit.worker.initialize(self.lit_config, {})

        try:
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
        except KeyboardInterrupt:
            print("\n[EZH Lit] Interrupted by user! Stopping test run...")

    lit.run.Run._execute = ezh_run_execute

    # ==================================================
    # 3. Execute Lit Main
    # ==================================================
    print("[EZH Lit] Monkey-patching complete! Igniting Lit Main...")
    import lit.main

    # Set up arguments.
    # CRITICAL: We enforce "-j 1" to guarantee sequential execution over single JTAG!
    sys.argv = [
        "lit",
        "-j", "1",
        "-v"
    ]

    # Forward the custom lit arguments (excluding --build-dir)
    sys.argv.extend(remaining_args)

    # Append the default test directory only if no positional arguments were provided
    if not positional_args:
        sys.argv.append(build_dir)

    try:
        lit.main.main()
    except KeyboardInterrupt:
        print("\n[EZH Lit] Interrupted by user!")
    finally:
        print("\n[EZH Lit] Detaching from JTAG remote...")
        process.Detach()

if __name__ == "__main__":
    run_ezh_lit(sys.argv[1:])
