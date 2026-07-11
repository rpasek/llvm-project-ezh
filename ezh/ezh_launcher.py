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

import time
import lldb

POLL_INTERVAL_SEC = 0.1

# NXP SmartDMA / EZH Peripheral Register Base, Offsets, and Keys
# (from EZHRegisters.h)
EZHB_BASE = 0x40027000
EZHB_BOOT_OFFSET = 0x20
EZHB_CTRL_OFFSET = 0x24
EZHB_PENDTRAP_OFFSET = 0x48

EZHB_BOOT_ADDR = EZHB_BASE + EZHB_BOOT_OFFSET  # 0x40027020
EZHB_CTRL_ADDR = EZHB_BASE + EZHB_CTRL_OFFSET  # 0x40027024
EZHB_PENDTRAP_ADDR = EZHB_BASE + EZHB_PENDTRAP_OFFSET  # 0x40027048

EZHB_CTRL_WRITE_KEY = 0xC0DE0000
EZHB_CTRL_START_KEY = EZHB_CTRL_WRITE_KEY | 0x11  # 0xC0DE0011

# RSTCTL0 Peripheral Reset Control Registers for SMARTDMA (from PERI_RSTCTL0.h)
RSTCTL0_BASE = 0x40000000
RSTCTL0_PRSTCTL0_SET_OFFSET = 0x40
RSTCTL0_PRSTCTL0_CLR_OFFSET = 0x70

RSTCTL0_PRSTCTL0_SET_ADDR = (
    RSTCTL0_BASE + RSTCTL0_PRSTCTL0_SET_OFFSET
)  # 0x40000040
RSTCTL0_PRSTCTL0_CLR_ADDR = (
    RSTCTL0_BASE + RSTCTL0_PRSTCTL0_CLR_OFFSET
)  # 0x40000070

RSTCTL0_PRSTCTL0_SMARTDMA_MASK = 0x40000000  # 1 << 30


def get_symbol(target, symbol_name, module=None):
    if module is None:
        module = target.GetModuleAtIndex(0)
    if not module or not module.IsValid():
        return None, 0
    sym = module.FindSymbol(symbol_name, lldb.eSymbolTypeAny)
    if sym.IsValid():
        return sym.GetStartAddress().GetLoadAddress(target), sym.GetSize()
    return None, 0


def connect(
    dummy_elf, url="connect://localhost:3333", plugin_name="ezh-remote"
):
    """Creates an LLDB debugger instance, registers target architecture
    using dummy_elf, connects to the remote server, and removes the
    initial startup module.

    Returns:
        tuple: (debugger, target, process, error_msg)
    """
    debugger = lldb.SBDebugger.Create()
    target = debugger.CreateTarget(dummy_elf)
    if not target or not target.IsValid():
        return (
            None,
            None,
            None,
            f"Failed to create target with ELF: {dummy_elf}",
        )

    error = lldb.SBError()
    process = target.ConnectRemote(
        debugger.GetListener(), url, plugin_name, error
    )
    if not error.Success() or not process.IsValid():
        return (
            None,
            None,
            None,
            f"Remote connection failed: {error.GetCString()}",
        )

    if target.GetNumModules() > 0:
        target.RemoveModule(target.GetModuleAtIndex(0))

    return debugger, target, process, None


def run_test(
    debugger, target, process, test_path, timeout=20.0, stdin_data=None
):
    """Resets EZH hardware, loads the target ELF module into memory,
    resolves symbols, initializes execution state, ignites the core,
    and polls for execution completion.

    Returns:
        tuple: (exc_val, stdout_str, error_msg)
            - exc_val: The integer exit status read from exc_signal on success,
            or 0xFFFFFFFF on launch failure.
            - stdout_str: Contents of stdout_buffer read on completion, or empty
            string on launch failure.
            - error_msg: None on success, or a string describing the error on
            failure.
    """
    exc_val = 0xFFFFFFFF
    error = lldb.SBError()
    res = lldb.SBCommandReturnObject()
    interpreter = debugger.GetCommandInterpreter()

    # Disable memory cache to ensure reliable reads of exc_signal during polling
    interpreter.HandleCommand(
        "settings set target.process.disable-memory-cache true", res
    )

    # Toggle EZH hardware reset via atomic SET/CLR registers
    process.WriteMemory(
        RSTCTL0_PRSTCTL0_SET_ADDR,
        RSTCTL0_PRSTCTL0_SMARTDMA_MASK.to_bytes(4, "little"),
        error,
    )
    process.WriteMemory(
        RSTCTL0_PRSTCTL0_CLR_ADDR,
        RSTCTL0_PRSTCTL0_SMARTDMA_MASK.to_bytes(4, "little"),
        error,
    )
    if not error.Success():
        return (
            exc_val,
            "",
            f"Hardware reset toggle failed: {error.GetCString()}",
        )

    exe_ctx = lldb.SBExecutionContext(target)
    interpreter.HandleCommand(
        f"target modules load --load --file {test_path} --slide 0", exe_ctx, res
    )
    if not res.Succeeded():
        return exc_val, "", f"LLDB modules loader failed: {res.GetError()}"

    # Resolve symbol addresses inside LLDB
    exc_signal_addr, _ = get_symbol(target, "exc_signal")
    if exc_signal_addr is None:
        return exc_val, "", "Failed to resolve exc_signal address"

    # Stdin Injection (Only performed if stdin_buffer exists and
    # stdin_data is provided)
    if stdin_data is not None:
        if isinstance(stdin_data, str):
            stdin_data = stdin_data.encode("utf-8")
        stdin_buffer_addr, buf_size = get_symbol(target, "stdin_buffer")
        if stdin_buffer_addr is not None:
            len_addr, _ = get_symbol(target, "stdin_buffer_len")
            idx_addr, _ = get_symbol(target, "stdin_buffer_idx")
            if len_addr and idx_addr:
                data_slice = stdin_data[:buf_size]
                process.WriteMemory(stdin_buffer_addr, data_slice, error)
                process.WriteMemory(
                    len_addr, len(data_slice).to_bytes(4, "little"), error
                )
                process.WriteMemory(idx_addr, (0).to_bytes(4, "little"), error)

    # Initialize exc_signal to 0xFFFFFFFF
    process.WriteMemory(exc_signal_addr, exc_val.to_bytes(4, "little"), error)
    if not error.Success():
        return exc_val, "", f"exc_signal init failed: {error.GetCString()}"

    # Clear bitslice
    process.WriteMemory(
        EZHB_PENDTRAP_ADDR, (0x00000080).to_bytes(4, "little"), error
    )

    # Resolve ELF entry point and set boot address register
    module = target.GetModuleAtIndex(0)
    entry = (
        module.GetObjectFileEntryPointAddress()
        if module and module.IsValid()
        else None
    )
    start_addr = (
        entry.GetLoadAddress(target) if entry and entry.IsValid() else None
    )
    if start_addr is None or start_addr == lldb.LLDB_INVALID_ADDRESS:
        return exc_val, "", "Failed to resolve ELF entry point address"
    process.WriteMemory(
        EZHB_BOOT_ADDR, (start_addr).to_bytes(4, "little"), error
    )
    if not error.Success():
        return exc_val, "", f"start PC load failed: {error.GetCString()}"

    # Trigger ignition
    process.WriteMemory(
        EZHB_CTRL_ADDR, EZHB_CTRL_START_KEY.to_bytes(4, "little"), error
    )
    if not error.Success():
        return exc_val, "", f"ignition failed: {error.GetCString()}"

    # Poll exc_signal up to max_polls
    polls = 0
    max_polls = int(timeout / POLL_INTERVAL_SEC)

    while polls < max_polls:
        time.sleep(POLL_INTERVAL_SEC)
        val_bytes = process.ReadMemory(exc_signal_addr, 4, error)
        if error.Success():
            exc_val = int.from_bytes(val_bytes, "little")
        # Break immediately only on exit statuses (0xCAFEBABE or 0xDEADxxxx)
        if (exc_val & 0xFFFF0000) == 0xDEAD0000 or exc_val == 0xCAFEBABE:
            break
        polls += 1

    stdout_str = ""
    stdout_buffer_addr, buf_size = get_symbol(target, "stdout_buffer")
    if stdout_buffer_addr is not None:
        raw_str_bytes = process.ReadMemory(stdout_buffer_addr, buf_size, error)
        if error.Success():
            null_idx = raw_str_bytes.find(b"\x00")
            if null_idx != -1:
                stdout_str = raw_str_bytes[:null_idx].decode(
                    "utf-8", errors="ignore"
                )
            else:
                stdout_str = raw_str_bytes.decode("utf-8", errors="ignore")

    return exc_val, stdout_str, None
