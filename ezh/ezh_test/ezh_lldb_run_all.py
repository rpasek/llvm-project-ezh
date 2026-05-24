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
import os
import re
import time

def get_symbol_addr_lldb(debugger, symbol_name, module_name=None):
    target = debugger.GetSelectedTarget()
    if not target.IsValid():
        return None
        
    # Find the specific module by filename
    module = None
    if module_name:
        for m in target.module_iter():
            if os.path.basename(m.GetFileSpec().GetFilename()) == module_name:
                module = m
                break
    else:
        module = target.GetExecutableModule()
        
    if not module or not module.IsValid():
        return None
        
    # 1. Try to find it as a function/entry symbol
    symbol = module.FindSymbol(symbol_name, lldb.eSymbolTypeAny)
    if symbol.IsValid():
        addr = symbol.GetStartAddress().GetLoadAddress(target)
        if addr != lldb.LLDB_INVALID_ADDRESS:
            return addr
            
    # 2. Try to find it as a global variable (like exc_signal)
    vars = module.FindGlobalVariables(target, symbol_name, 1)
    if vars.GetSize() > 0:
        var = vars.GetValueAtIndex(0)
        addr = var.GetAddress().GetLoadAddress(target)
        if addr != lldb.LLDB_INVALID_ADDRESS:
            return addr
            
    return None

def run_all_tests(debugger):
    print("\n================================================")
    print("              EZH LLDB TEST RUNNER              ")
    print("================================================\n")
    
    # Discover all compiled ELF test cases recursively inside the build directory!
    execute_dir = os.environ.get("EZH_TEST_EXECUTE_DIR")
    if not execute_dir or not os.path.exists(execute_dir):
        print(f"Error: Execute directory {execute_dir} does not exist!")
        return
        
    tests = []
    for root, dirs, files in os.walk(execute_dir):
        # Exclude benchmarks and un-linked/intermediate build folders dynamically!
        if "Benchmarks" in root or "CMakeFiles" in root or "Output" in root or "tools" in root:
            continue
        for f in files:
            if f.endswith(".o"):
                continue
            # Check if the file is a valid ELF executable by reading the first 4 bytes!
            full_path = os.path.join(root, f)
            try:
                with open(full_path, 'rb') as fp:
                    header = fp.read(4)
                    if header == b'\x7fELF':
                        tests.append(full_path)
            except:
                pass
                
    if not tests:
        print(f"Error: No valid ELF test cases discovered inside {execute_dir}!")
        return
        
    print(f"Discovered {len(tests)} tests in the build-test-suite (Filtered to target list).")
    
    # 2. Initialize Command Interpreter
    interpreter = debugger.GetCommandInterpreter()
    res = lldb.SBCommandReturnObject()
    
    # Pre-index all reference output files recursively ONCE at startup!
    # Maps 'SingleSource/UnitTests/sumarray' -> absolute path to prevent duplicate collisions across folders!
    reference_outputs_cache = {}
    source_test_suite = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "llvm_test", "llvm-test-suite"))
    for root, dirs, files in os.walk(source_test_suite):
        for f in files:
            if f.endswith(".reference_output"):
                base_name = f.rsplit(".reference_output", 1)[0]
                # Standardize the key relative to the test suite root directory VMA
                full_ref_path = os.path.join(root, base_name)
                rel_key = os.path.relpath(full_ref_path, source_test_suite)
                reference_outputs_cache[rel_key] = os.path.abspath(os.path.join(root, f))
                
    # Read absolute build root EZH_BUILD_DIR to preserve intermediate directories cleanly
    build_root = os.environ.get("EZH_BUILD_DIR")
    if not build_root:
        build_root = os.path.dirname(execute_dir) # Fallback
        
    # Load the first target ELF dynamically at startup to register EZH architecture (Only once!)
    first_test = tests[0]
    print(f"Registering EZH base target: {first_test}")
    target = debugger.CreateTarget(first_test)
    if not target or not target.IsValid():
        print(f"Error: Failed to create target: {first_test}")
        return
        
    # 3. Connect JTAG remotely ONCE at startup natively via Python SBTarget API!
    print("Connecting to OpenOCD JTAG remote...")
    error = lldb.SBError()
    process = target.ConnectRemote(debugger.GetListener(), "connect://localhost:3333", "ezh-remote", error)
    if not error.Success() or not process.IsValid():
        print(f"Error: Failed to connect JTAG remote natively: {error.GetCString()}")
        return
        
    # Disable LLDB memory cache to prevent stale reads of exc_signal during polling
    interpreter.HandleCommand('settings set target.process.disable-memory-cache true', res)
        
    # Resolve active target process context
    if not process or not process.IsValid():
        print("Error: Failed to retrieve active JTAG process context!")
        return
        
    error = lldb.SBError()
    
    # Halt EZH core natively
    process.WriteMemory(0x40027024, (0xC0DE0000).to_bytes(4, 'little'), error)
    if not error.Success():
        print(f"Error: Failed to halt core at startup: {error.GetCString()}")
        return
        
    passed_count = 0
    failed_tests = []
    
    summary_path = os.path.join(os.path.dirname(__file__), "ezh_regression_summary.txt")
    try:
        with open(summary_path, "w") as sf:
            sf.write("================================================\n")
            sf.write("     EZH JTAG DYNAMIC REGRESSION RUN LOGGER      \n")
            sf.write("================================================\n")
            sf.write(f" Discovered: {len(tests)} tests\n\n")
    except Exception as init_err:
        print(f"[Warning] Failed to initialize log file: {init_err}")
    
    try:
        for idx, test_path in enumerate(tests):
            test_name = os.path.basename(test_path)
            print(f"[{idx + 1}/{len(tests)}] Loading & executing {test_name}... ", end="", flush=True)
            
            # A. Add the new test ELF as a module natively using Python SB API (skip if already loaded as base startup target!)
            module = None
            if test_path != first_test:
                module = target.AddModule(test_path, None, None)
                if not module or not module.IsValid():
                    raise Exception(f"Failed to natively import module: {test_path}")
                    
            try:
                # B. Halt EZH core natively to unlock RAM loading
                process.WriteMemory(0x40027024, (0xC0DE0000).to_bytes(4, 'little'), error)
                if not error.Success():
                    raise Exception(f"JTAG Halt failed: {error.GetCString()}")
                    
                # C. Toggle EZH hardware reset natively via direct memory writes
                val_bytes = process.ReadMemory(0x40000010, 4, error)
                if not error.Success():
                    raise Exception(f"Failed to read reset register: {error.GetCString()}")
                val = int.from_bytes(val_bytes, 'little')
                process.WriteMemory(0x40000010, (val | (1 << 30)).to_bytes(4, 'little'), error)
                process.WriteMemory(0x40000010, (val & ~(1 << 30)).to_bytes(4, 'little'), error)
                
                exe_ctx = lldb.SBExecutionContext(target)
                interpreter.HandleCommand(f'target modules load --load --file {test_path} --slide 0', exe_ctx, res)
                if not res.Succeeded():
                    raise Exception(f"LLDB modules loader failed: {res.GetError()}")
                    
                # E. Natively resolve dynamic symbol addresses inside LLDB in microseconds AFTER loading!
                exc_signal_addr = get_symbol_addr_lldb(debugger, "exc_signal", test_name)
                start_addr = get_symbol_addr_lldb(debugger, "_start", test_name)
                if exc_signal_addr is None or start_addr is None:
                    raise Exception("Failed to resolve dynamic symbol VMA addresses (_start, exc_signal) inside LLDB!")
                    
                # F. Directly initialize exc_signal to 0xFFFFFFFF via JTAG write
                process.WriteMemory(exc_signal_addr, (0xFFFFFFFF).to_bytes(4, 'little'), error)
                if not error.Success():
                    raise Exception(f"JTAG exc_signal init failed: {error.GetCString()}")
                
                # G. Clear bitslice natively
                process.WriteMemory(0x40027048, (0x00000080).to_bytes(4, 'little'), error)
                
                # H. Set PC start register to resolved start address natively
                process.WriteMemory(0x40027024, (0xC0DE0000).to_bytes(4, 'little'), error) # Safety Halt prior to PC write!
                process.WriteMemory(0x40027020, (start_addr).to_bytes(4, 'little'), error)
                if not error.Success():
                    raise Exception(f"JTAG start PC load failed: {error.GetCString()}")
                    
                # I. Trigger JTAG ignition natively
                process.WriteMemory(0x40027024, (0xC0DE0011).to_bytes(4, 'little'), error)
                if not error.Success():
                    raise Exception(f"JTAG ignition failed: {error.GetCString()}")
                    
                # J. Poll exc_signal dynamically every 100ms up to a 120.0s timeout natively!
                exc_val = 0xFFFFFFFF
                polls = 0
                max_polls = 200  # 120.0 seconds
                
                while polls < max_polls:
                    time.sleep(0.1)
                    val_bytes = process.ReadMemory(exc_signal_addr, 4, error)
                    if error.Success():
                        exc_val = int.from_bytes(val_bytes, 'little')
                    # Break immediately on any valid, useful native exit status!
                    if exc_val != 0xFFFFFFFF and exc_val != 0xDEADDEAD and exc_val != 0x55555555 and exc_val != 0x0deadb55:
                        break
                    polls += 1
            except Exception as run_err:
                exc_val = 0xDEADC0DE
                print(f"[Execution Error] {run_err} ", end="")
                
            # L. Validate results - EVERY successful test case MUST exit with exc_signal == 0xCAFEBABE!
            passed = (exc_val == 0xCAFEBABE)
            if not passed:
                fail_reason = f"Abort status: 0x{exc_val:08x}"
            else:
                # Dynamically strip CMake-injected unique target prefixes to resolve the original repository filename!
                clean_name = test_name
                prefixes_to_strip = ["Regression-C++-", "Regression-C-", "gcc-c-torture-", "GCC-C-execute-"]
                for prefix in prefixes_to_strip:
                    if clean_name.startswith(prefix):
                        clean_name = clean_name[len(prefix):]
                        break
                        
                # Resolve the relative path of the executable cleanly relative to the build root
                relative_path = os.path.relpath(test_path, build_root)
                relative_dir = os.path.dirname(relative_path)
                rel_key = os.path.join(relative_dir, clean_name)
                

                
                # Standard exit success confirmed! Only perform output validation if a reference output exists in cache!
                ref_out_path = reference_outputs_cache.get(rel_key)

                if ref_out_path:
                    try:
                        # Read EZH SRAM printf_buffer natively via JTAG
                        stdout_str = ""
                        printf_buffer_addr = get_symbol_addr_lldb(debugger, "printf_buffer", test_name)
                        if printf_buffer_addr is not None:
                            raw_str_bytes = process.ReadMemory(printf_buffer_addr, 2048, error)
                            if error.Success():
                                null_idx = raw_str_bytes.find(b'\x00')
                                if null_idx != -1:
                                    stdout_str = raw_str_bytes[:null_idx].decode('utf-8', errors='ignore')
                                else:
                                    stdout_str = raw_str_bytes.decode('utf-8', errors='ignore')
                                    
                        with open(ref_out_path, 'r', encoding='utf-8', errors='ignore') as rf:
                            expected_out = rf.read().strip()
                            
                        # Clean up any carriage returns and trailing spaces
                        clean_observed = "\n".join(line.strip() for line in stdout_str.strip().splitlines())
                        clean_expected = "\n".join(line.strip() for line in expected_out.strip().splitlines())
                        
                        if clean_observed == clean_expected:
                            passed = True
                        else:
                            passed = False
                            fail_reason = f"Output Mismatch!\n  Expected:\n{expected_out}\n  Observed:\n{stdout_str}"
                    except Exception as ref_err:
                        passed = False
                        fail_reason = f"Failed to read reference output: {ref_err}"
                        
            if passed:
                print(f"PASSED (exc_signal: 0x{exc_val:08x})")
                passed_count += 1
                try:
                    with open(summary_path, "a") as sf:
                        sf.write(f"[{idx + 1}/{len(tests)}] [PASSED] {test_name} (exc_signal: 0x{exc_val:08x})\n")
                except:
                    pass
            else:
                print(f"FAILED (Reason: {fail_reason})")
                failed_tests.append((test_name, exc_val, fail_reason))
                try:
                    with open(summary_path, "a") as sf:
                        sf.write(f"[{idx + 1}/{len(tests)}] [FAILED] {test_name} (exc_signal: 0x{exc_val:08x}, Reason: {fail_reason})\n")
                except:
                    pass
                    
            # K. Clean remove the test ELF module natively after all validation checks have fully completed!
            # This ensures symbols remain valid during printf_buffer resolution!
            if module and module.IsValid():
                target.RemoveModule(module)
    except Exception as e:
        import traceback
        print(f"\n[JTAG Exception] {e}")
        traceback.print_exc()
    finally:
        # 1. Print cleanly to console as fallback
        print("\n================================================")
        print("               RUN SUMMARY                      ")
        print("================================================")
        print(f" Discovered: {len(tests)} tests")
        print(f" Passed:     {passed_count}")
        print(f" Failed:     {len(failed_tests)}")
        if failed_tests:
            print("\nFailing tests:")
            for name, val, reason in failed_tests:
                print(f"  * {name} (exc_signal: 0x{val:08x}, Reason: {reason.replace(chr(10), ' ')})")
        print("================================================")
        
        # 2. Write the complete summary safely to a local text file!
        try:
            summary_path = os.path.join(os.path.dirname(__file__), "ezh_regression_summary.txt")
            with open(summary_path, "a") as sf:
                sf.write("\n================================================\n")
                sf.write("               RUN SUMMARY                      \n")
                sf.write("================================================\n")
                sf.write(f" Discovered: {len(tests)} tests\n")
                sf.write(f" Passed:     {passed_count}\n")
                sf.write(f" Failed:     {len(failed_tests)}\n")
                if failed_tests:
                    sf.write("\nFailing tests:\n")
                    for name, val, reason in failed_tests:
                        sf.write(f"  * {name} (exc_signal: 0x{val:08x}, Reason: {reason.replace(chr(10), ' ')}).test\n")
                sf.write("================================================\n")
            print(f"\n[Summary saved safely to: ezh_regression_summary.txt]")
        except Exception as file_err:
            print(f"[Warning] Failed to write summary file: {file_err}")
            
        import sys
        sys.stdout.flush()
        
        print("\nDetaching from JTAG remote...")
        try:
            process.Detach()
        except Exception as detach_err:
            print(f"[Warning] Detach failed: {detach_err}")
            
    # Terminate LLDB cleanly
    interpreter.HandleCommand('quit', res)

def __lldb_init_module(debugger, internal_dict):
    run_all_tests(debugger)
