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
import time

def find_symbol(target, module, name):
    """Natively resolves a symbol address from a specific module spec."""
    sym = module.FindSymbol(name, lldb.eSymbolTypeAny)
    if sym.IsValid():
        return sym.GetStartAddress().GetLoadAddress(target)
    # Fallback to global variables if symbol is not in symtab (like BSS variables)
    vars = module.FindGlobalVariables(target, name, 1)
    if vars.GetSize() > 0:
        var = vars.GetValueAtIndex(0)
        addr = var.GetAddress().GetLoadAddress(target)
        if addr != lldb.LLDB_INVALID_ADDRESS:
            return addr
    return None

def run_ezh_core(debugger, test_path, dummy_elf=None, poll_interval=0.05, max_timeout=20.0, target=None, process=None, stdin_file=None):
    """
    Core JTAG execution engine shared between Lit and Single-Test runners.
    Supports both Standalone (connect-run-detach) and Shared (reuse connection) modes.

    Args:
        debugger: The active SBDebugger instance.
        test_path: Path to the test ELF binary to run.
        dummy_elf: Optional path to a base dummy ELF to use the "Dummy Target" architecture.
        poll_interval: Sleep time between JTAG polls in seconds.
        max_timeout: Maximum execution timeout in seconds.
        target: Optional existing SBTarget to reuse (Shared Mode).
        process: Optional existing SBProcess to reuse (Shared Mode).
        stdin_file: Optional path to a file containing stdin data to inject.

    Returns:
        tuple: (stdout_str, exit_code, exc_val)
            - stdout_str: Contents of the printf_buffer.
            - exit_code: 0 for success, 1 for failure/timeout.
            - exc_val: The final value read from exc_signal.
    """
    interpreter = debugger.GetCommandInterpreter()
    res = lldb.SBCommandReturnObject()
    error = lldb.SBError()
    
    # Infallible Cache Suppression: Apply setting before Target and Process instantiation!
    interpreter.HandleCommand('settings set target.process.disable-memory-cache true', res)

    is_external_target = (target is not None and process is not None)
    is_base = False
    module = None

    # Create or Reuse Target
    if is_external_target:
        # Shared Mode: Reuse existing target and process
        is_base = (os.path.abspath(test_path) == os.path.abspath(dummy_elf)) if dummy_elf else False
        if not is_base:
            module = target.AddModule(test_path, None, None)
            if not module or not module.IsValid():
                return "", f"Failed to add ELF module to shared target: {test_path}", 1, 0xFFFFFFFF
        else:
            module = target.GetModuleAtIndex(0)
    else:
        # Standalone Mode: Create a new target and connect
        if dummy_elf:
            print(f"[JTAG Engine] Creating target with base ELF: {dummy_elf}")
            target = debugger.CreateTarget(dummy_elf)
            if not target or not target.IsValid():
                return "", f"Failed to create target for base ELF: {dummy_elf}", 1, 0xFFFFFFFF

            module = target.AddModule(test_path, None, None)
            if not module or not module.IsValid():
                return "", f"Failed to add ELF module: {test_path}", 1, 0xFFFFFFFF
        else:
            print(f"[JTAG Engine] Creating target directly with test ELF: {test_path}")
            target = debugger.CreateTarget(test_path)
            if not target or not target.IsValid():
                return "", f"Failed to create target: {test_path}", 1, 0xFFFFFFFF
            module = target.GetModuleAtIndex(0)
            if not module or not module.IsValid():
                return "", "Failed to get target module", 1, 0xFFFFFFFF

        # Connect JTAG Remote (using the required "ezh-remote" plugin!)
        print("[JTAG Engine] Connecting to OpenOCD JTAG remote...")
        process = target.ConnectRemote(debugger.GetListener(), "connect://localhost:3333", "ezh-remote", error)
        if not error.Success() or not process.IsValid():
            return "", f"Failed to connect JTAG remote: {error.GetCString()}", 1, 0xFFFFFFFF



    try:
        # Halt EZH Core Natively to unlock RAM (using the exact Lit sequence)
        process.WriteMemory(0x40027024, (0xC0DE0000).to_bytes(4, 'little'), error)
        if not error.Success():
            raise Exception(f"Failed to halt core at startup: {error.GetCString()}")

        # Toggle EZH Hardware Reset Natively
        val_bytes = process.ReadMemory(0x40000010, 4, error)
        if not error.Success():
            raise Exception(f"Failed to read reset register: {error.GetCString()}")
        val = int.from_bytes(val_bytes, 'little')
        process.WriteMemory(0x40000010, (val | (1 << 30)).to_bytes(4, 'little'), error)
        process.WriteMemory(0x40000010, (val & ~(1 << 30)).to_bytes(4, 'little'), error)
        if not error.Success():
            raise Exception(f"JTAG Reset Toggle failed: {error.GetCString()}")

        # Load ELF Sections Natively onto SRAM
        exe_ctx = lldb.SBExecutionContext(target)
        interpreter.HandleCommand(
            f'target modules load --load --file {test_path} --slide 0',
            exe_ctx, res)
        if not res.Succeeded():
            # Diagnostic: Print all modules currently registered in the target!
            print(f"[JTAG Engine] Diagnostic: Load failed. Active target modules:")
            for idx, m in enumerate(target.module_iter()):
                file_spec = m.GetFileSpec()
                print(f"  [{idx}] {file_spec.GetDirectory()}/{file_spec.GetFilename()}")
            raise Exception(f"LLDB modules loader failed: {res.GetError()}")

        # Resolve Dynamic Symbols Addresses
        exc_signal_addr = find_symbol(target, module, "exc_signal")
        start_addr = find_symbol(target, module, "_start")
        if exc_signal_addr is None or start_addr is None:
            raise Exception("Failed to resolve essential symbols (_start, exc_signal)!")

        # Stdin Injection (Only performed if stdin_buffer exists and stdin_file is provided)
        stdin_buffer_addr = find_symbol(target, module, "stdin_buffer")
        if stdin_buffer_addr is not None and stdin_file and os.path.exists(stdin_file):
            stdin_buffer_len_addr = find_symbol(target, module, "stdin_buffer_len")
            stdin_buffer_idx_addr = find_symbol(target, module, "stdin_buffer_idx")
            if stdin_buffer_len_addr is not None and stdin_buffer_idx_addr is not None:
                with open(stdin_file, "rb") as f_in:
                    stdin_data = f_in.read()
                if len(stdin_data) > 1024:
                    stdin_data = stdin_data[:1024]
                print(f"[JTAG Engine] Injecting {len(stdin_data)} bytes of stdin from {stdin_file}...")
                process.WriteMemory(stdin_buffer_addr, stdin_data, error)
                process.WriteMemory(stdin_buffer_len_addr, len(stdin_data).to_bytes(4, 'little'), error)
                process.WriteMemory(stdin_buffer_idx_addr, (0).to_bytes(4, 'little'), error)

        # Directly Initialize exc_signal to 0xFFFFFFFF
        process.WriteMemory(exc_signal_addr, (0xFFFFFFFF).to_bytes(4, 'little'), error)
        if not error.Success():
            raise Exception(f"JTAG exc_signal init failed: {error.GetCString()}")

        # Clear Bitslice Natively
        process.WriteMemory(0x40027048, (0x00000080).to_bytes(4, 'little'), error)

        # Set PC start register (No pre-PC safety halt, matching Lit exactly!)
        process.WriteMemory(0x40027020, (start_addr).to_bytes(4, 'little'), error)
        if not error.Success():
            raise Exception(f"JTAG start PC load failed: {error.GetCString()}")

        # Ignite EZH Core (0xC0DE0011 magic prefix required!)
        process.WriteMemory(0x40027024, (0xC0DE0011).to_bytes(4, 'little'), error)
        if not error.Success():
            raise Exception(f"JTAG ignition failed: {error.GetCString()}")

        # Poll exc_signal Dynamically
        exc_val = 0xFFFFFFFF
        polls = 0
        max_polls = int(max_timeout / poll_interval)

        while polls < max_polls:
            time.sleep(poll_interval)
            val_bytes = process.ReadMemory(exc_signal_addr, 4, error)
            if error.Success():
                exc_val = int.from_bytes(val_bytes, 'little')
                # ONLY terminate early on official termination codes (success or explicit abort)
                if exc_val == 0xCAFEBABE or exc_val == 0xDEADC0DE:
                    break
            else:
                print(f"exc_signal read fail")
            polls += 1

        # If Timeout or Abnormal Stop
        if exc_val != 0xCAFEBABE:
            val_bytes = process.ReadMemory(exc_signal_addr, 4, error)

        # Read printf_buffer on completion
        stdout_str = ""
        printf_buffer_addr = find_symbol(target, module, "printf_buffer")
        if printf_buffer_addr is not None:
            raw_str_bytes = process.ReadMemory(printf_buffer_addr, 2048, error)
            if error.Success():
                null_idx = raw_str_bytes.find(b'\x00')
                if null_idx != -1:
                    stdout_str = raw_str_bytes[:null_idx].decode('utf-8', errors='ignore')
                else:
                    stdout_str = raw_str_bytes.decode('utf-8', errors='ignore')

        # Determine final status
        if exc_val == 0xCAFEBABE:
            return stdout_str, 0, exc_val
        else:
            return stdout_str, 1, exc_val

    except Exception as e:
        import traceback
        print(f"\n[JTAG Engine Exception] {e}")
        traceback.print_exc()
        return str(e), 1, 0xFFFFFFFF
    finally:
        if is_external_target:
            # Shared Mode: Clean up the module from the shared target, but do NOT detach!
            if not is_base and module and module.IsValid():
                target.RemoveModule(module)
            # Force EZH core back to a safe, halted state
            try:
                process.WriteMemory(0x40027024, (0xC0DE0000).to_bytes(4, 'little'), error)
            except:
                pass
        else:
            # Standalone Mode: Full Halt and Detach
            print("[JTAG Engine] Detaching from JTAG remote...")
            try:
                process.WriteMemory(0x40027024, (0xC0DE0000).to_bytes(4, 'little'), error)
                process.Detach()
            except Exception as detach_err:
                print(f"[JTAG Engine Warning] Detach failed: {detach_err}")
