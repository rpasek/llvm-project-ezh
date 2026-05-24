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

import subprocess
import pexpect
import time
import sys
import os

class TeeLogger:
    def __init__(self, file_obj, terminal_stream):
        self.file_obj = file_obj
        self.terminal_stream = terminal_stream
    def write(self, data):
        self.file_obj.write(data)
        self.file_obj.flush()
        self.terminal_stream.write(data)
        self.terminal_stream.flush()
    def flush(self):
        self.file_obj.flush()
        self.terminal_stream.flush()

def run_comprehensive_tests():
    # Open log file on disk for all debugger and console diagnostics!
    log_file = open("lldb_comprehensive.log", "w")

    # Redirect sys.stdout to TeeLogger to write to both console and file!
    sys.stdout = TeeLogger(log_file, sys.stdout)

    print("\n================================================================================")
    print("=== EZH COMPREHENSIVE DEBUGGER VERIFICATION SUITE (PURE INTERACTIVE)         ===")
    print("================================================================================\n")

    # Build clean binaries first
    print("=== Building target binaries ===")
    subprocess.run(['killall', '-9', 'lldb', 'lldb-server'], capture_output=True)
    subprocess.run(['../../build/bin/clang', '-target', 'ezh-none-elf', '-g', '-Os', '-ffunction-sections', '-fdata-sections', '-Wall', '-Wextra', '-Werror', '-Wundef', '-isystem', '../../build/libc/libc/include', '-I', '../../lldb/source/Plugins/Process/EZH/', '-D__TEST__', '../crt0.c', '-c', '-o', 'out/crt0.o', '-fno-builtin'], check=True)
    subprocess.run(['../../build/bin/clang', '-target', 'ezh-none-elf', '-g', '-Os', '-ffunction-sections', '-fdata-sections', '-Wall', '-Wextra', '-Werror', '-Wundef', '-isystem', '../../build/libc/libc/include', '-I', '.', '-I', '../ezh_test', '-D__TEST__', 'ezh_debug_test.c', '-c', '-o', 'out/ezh_debug_test.o', '-fno-builtin'], check=True)
    subprocess.run([
        '../../build/bin/ld.lld',
        '-L../../build/libc/lib',
        '-L../../build/libc/libc/lib',
        '-T', '../llvm_test/smartdma_large.ld',
        '--gc-sections',
        '--discard-locals',
        'out/crt0.o',
        'out/ezh_debug_test.o',
        '../../build/compiler-rt/lib/linux/libclang_rt.builtins-ezh.a',
        '../../build/libc/libc/lib/libc.a',
        '../../build/libc/libc/lib/libm.a',
        '-lc++',
        '-lc++abi',
        '-lunwind',
        '-Map=out/ezh_debug_test.map',
        '-o', 'out/ezh_debug_test.elf'
    ], check=True)
    with open('out/ezh_debug_test.disasm', 'w') as f:
        subprocess.run(['../../build/bin/llvm-objdump', '-d', '--triple=ezh', 'out/ezh_debug_test.elf'], stdout=f, check=True)

    # Launch LLVM debugger interactively!
    child = pexpect.spawn('../../build/bin/lldb out/ezh_debug_test.elf', encoding='utf-8', timeout=15)
    # Redirect child log output to the log file in real-time!
    child.logfile = log_file

    # Disable ANSI color escape codes for perfectly clean logging!
    child.sendline('settings set use-color false')
    child.expect(r"\(lldb\)")

    # ------------------------------------------------------------------------------
    # PHASE 1: CONNECT AND IGNITION VERIFICATION
    # ------------------------------------------------------------------------------
    print("\n=== PHASE 1: Reset & Ignition Verification (stepi vs continue) ===\n")

    # Connect to JTAG
    child.sendline('process connect -p ezh-remote connect://localhost:3333')
    child.expect('Process 1 stopped')

    # Halt EZH core
    child.sendline('memory write -s 4 0x40027024 0xC0DE0000')
    child.expect(r"\(lldb\)")

    # Reset hardware
    child.sendline('expr *(volatile int *)0x40000010 |= (1 << 30)')
    child.expect(r"\(lldb\)")
    child.sendline('expr *(volatile int *)0x40000010 &= ~(1 << 30)')
    child.expect(r"\(lldb\)")

    # Load ELF module mappings
    child.sendline('target modules load --load --file ezh_debug_test.elf --slide 0')
    child.expect(r"\(lldb\)")

    # Clear variables
    child.sendline('expr exc_signal = 0')
    child.expect(r"\(lldb\)")

    # Clear bitslice
    child.sendline('memory write -s 4 0x40027048 0x00000080')
    child.expect(r"\(lldb\)")

    # Halt EZH core
    child.sendline('memory write -s 4 0x40027024 0xC0DE0010')
    child.expect(r"\(lldb\)")

    # Test 1A: Ignite core directly from reset by stepping (stepi) -> Must block it!
    print("\n[Test 1A: Cold-Stepping Safety Protection]")
    child.sendline('si')
    child.expect('Cannot step, stepi, next or finish a non running core')
    print("--> [PASS] Stepping from reset correctly blocked with safety message!")

    # Test 1B: Ignite core by continue to breakpoint (tests automatic step-plan cleanup!)
    print("\n[Test 1B: Target Ignition via Breakpoint Continue]")
    child.sendline('b test_1b_1c_ignition_and_corruption')
    child.expect('Breakpoint 1')
    child.sendline('continue')
    child.expect('stop reason = breakpoint 1.1')
    child.sendline('breakpoint disable')
    child.expect(r"\(lldb\)")
    print("--> [PASS] Halted target ignited successfully via continue!")

    # Test 1C: Step over register immediate loads and verify correct register reads
    print("\n[Test 1C: Step Over Register Immediate Loads]")
    # Step 9 times sequentially over immediate loads (r0-r6, gpo, gpd)
    for _ in range(9):
        child.sendline('si')
        child.expect('stop reason = instruction step into')
    child.sendline('register read r0 r1 r2 r3 r4 r5 r6 gpo gpd')
    child.expect('r0 = 0x00000011')
    child.expect('r1 = 0x00000022')
    child.expect('r2 = 0x00000033')
    child.expect('r3 = 0x00000044')
    child.expect('r4 = 0x00000055')
    child.expect('r5 = 0x00000066')
    child.expect('r6 = 0x00000077')
    child.expect('gpo = 0x00000099')
    child.expect('gpd = 0x000000aa')
    print("--> [PASS] Register modification tracking via stepi & register read successful!")

    # ------------------------------------------------------------------------------
    # PHASE 3: SUBROUTINE FLOW CONTROL (STEP, NEXT, FINISH)
    # ------------------------------------------------------------------------------
    print("\n=== PHASE 3: Subroutine Flow Control (step, next, finish) ===\n")

    child.sendline('b test_3_subroutine_step')
    child.expect('Breakpoint 2')
    child.sendline('continue')
    child.expect('stop reason = breakpoint 2.1')
    child.sendline('breakpoint disable')
    child.expect(r"\(lldb\)")

    # Step 1: step over function-entry NOPs to exc_signal line (Line 20)
    time.sleep(0.2)
    child.sendline('step')
    child.expect('stop reason = step')
    child.expect(r'-> 38')

    # Step 2: step over exc_signal to test_3_subroutine_step_target() call (Line 39)
    time.sleep(0.2)
    child.sendline('step')
    child.expect('stop reason = step')
    child.expect(r'-> 39')

    # VERIFY: exc_signal was updated to 0x88888888 in EZH RAM!
    child.sendline('print/x exc_signal')
    child.expect('0x88888888')
    print("--> [PASS] C-level stepping over assignment correctly updated exc_signal to 0x88888888!")

    # Step 3: step directly into subroutine test_3_subroutine_step_target()
    time.sleep(0.2)
    child.sendline('step')
    child.expect('stop reason = step')
    child.expect(r'frame #0: 0x[0-9a-f]+ ezh_debug_test.elf`test_3_subroutine_step_target')

    # Step 3.1: step directly to exc_signal assignment (line 28)
    time.sleep(0.2)
    child.sendline('step')
    child.expect('stop reason = step')
    child.expect(r'-> 28')

    # Step 3.3: step over exc_signal assignment inside subroutine to closing brace (line 29)
    time.sleep(0.2)
    child.sendline('step')
    child.expect('stop reason = step')
    child.expect(r'-> 29\s+\}')

    # VERIFY: exc_signal inside subroutine was updated to 0x99999999 in EZH RAM!
    child.sendline('print/x exc_signal')
    child.expect('0x99999999')
    print("--> [PASS] Stepping inside subroutine correctly updated exc_signal to 0x99999999!")

    # Finish out of test_3_subroutine_step_target to closing brace of caller (Line 40)
    time.sleep(0.2)
    child.sendline('finish')
    child.expect('stop reason = step out')
    child.expect(r'-> 40')

    # Step over next statement to exit
    time.sleep(0.2)
    child.sendline('next')
    child.expect('stop reason = step over')

    # ------------------------------------------------------------------------------
    # PHASE 3B: CONSECUTIVE STEPPING INSIDE CLEAN C CONDITIONAL BRANCHES
    # ------------------------------------------------------------------------------
    print("\n=== PHASE 3B: Consecutive Stepping inside Clean C Conditional Branches ===\n")

    child.sendline('b test_4_c_stepping')
    child.expect('Breakpoint 3')
    child.sendline('continue')
    child.expect('stop reason = breakpoint 3.1')
    child.sendline('breakpoint disable')
    child.expect(r"\(lldb\)")

    # Step 1: step over function-entry synchronization NOPs to variable declaration
    time.sleep(0.2)
    child.sendline('step')
    child.expect('stop reason = step')
    child.expect('ezh_debug_test.elf`test_4_c_stepping')

    # Step 2: step over variable declaration to the first conditional branch (x == 1)
    time.sleep(0.2)
    child.sendline('step')
    child.expect('stop reason = step')
    child.expect(r'if \(x == 1\)')

    # Perform 6 consecutive steps to step through both Taken and Not Taken conditional branches!
    print("\n[Test 3B: Consecutive Stepping inside C Conditional Branches (6 times)]")

    # Step 3 (x == 1 Taken): should step cleanly into the True block!
    time.sleep(0.2)
    child.sendline('step')
    child.expect('stop reason = step')
    child.expect(r'-> 211\s+exc_signal = 0xaaaa0001')
    print("--> [PASS] Step 1/6 (ZE=1 Taken Branch into True block) successful!")

    # Step 4 (Block exit): steps from exc_signal to the } else { block exit line
    time.sleep(0.2)
    child.sendline('step')
    child.expect('stop reason = step')

    # VERIFY: exc_signal in Taken block was written successfully!
    child.sendline('print/x exc_signal')
    child.expect('0xaaaa0001')
    print("--> [PASS] C-level conditional step correctly wrote exc_signal to 0xaaaa0001!")

    # Step 5 (Next Branch entry): steps from } else { to the next conditional branch (x == 2)
    time.sleep(0.2)
    child.sendline('step')
    child.expect('stop reason = step')
    child.expect(r'if \(x == 2\)')
    print("--> [PASS] Step 2/6 (Exit Taken Block to next branch) successful!")

    # Step 6 (x == 2 Not Taken): should step cleanly bypassing True block to the False block!
    time.sleep(0.2)
    child.sendline('step')
    child.expect('stop reason = step')
    child.expect(r'-> 219\s+exc_signal = 0xaaaa0004')
    print("--> [PASS] Step 3/6 (ZE=0 Not Taken Branch into False block) successful!")

    # Step 7 (Exit False Block): steps to the exit of the function }
    time.sleep(0.2)
    child.sendline('step')
    child.expect('stop reason = step')
    child.expect(r'-> 221\s+\}')

    # VERIFY: exc_signal in False block was written successfully!
    child.sendline('print/x exc_signal')
    child.expect('0xaaaa0004')
    print("--> [PASS] C-level conditional step correctly wrote exc_signal to 0xaaaa0004!")
    print("--> [PASS] Step 4/6 (Exit False Block) successful!")

    child.sendline('quit')
    child.sendline('Y')
    child.expect(pexpect.EOF)
    print("--> [PASS] Subroutine Flow Control and C Conditional stepping Successful!")

    # ------------------------------------------------------------------------------
    # PHASE 4: ASYNCHRONOUS INTERRUPT (CTRL-C)
    # ------------------------------------------------------------------------------
    print("\n=== PHASE 4: Asynchronous Interrupt (Ctrl-C) ===\n")
    child = pexpect.spawn('../../build/bin/lldb out/ezh_debug_test.elf', encoding='utf-8', timeout=15)
    child.logfile = log_file
    child.sendline('settings set use-color false')
    child.expect(r"\(lldb\)")
    child.sendline('process connect -p ezh-remote connect://localhost:3333')
    child.expect(r"\(lldb\)")

    # Reset hardware cleanly to clear debugger trap states!
    child.sendline('expr *(volatile int *)0x40000010 |= (1 << 30)')
    child.expect(r"\(lldb\)")
    child.sendline('expr *(volatile int *)0x40000010 &= ~(1 << 30)')
    child.expect(r"\(lldb\)")
    child.sendline('expr exc_signal = 0')
    child.expect(r"\(lldb\)")
    child.sendline('target modules load --load --file ezh_debug_test.elf --slide 0')
    child.expect(r"\(lldb\)")
    child.sendline('expr *(volatile int *)0x40027034 = (int)&debug_handler')
    child.expect(r"\(lldb\)")

    # Run EZH target cleanly
    child.sendline('continue')
    time.sleep(1.5)

    # Send Ctrl-C halt request
    child.sendcontrol('c')
    child.expect('Process 1 stopped')

    print("\n[Test 4: Consecutive Stepping (5 times) inside bitslice_handler after Ctrl-C]")

    # Step 1
    time.sleep(0.2)
    child.sendline('step')
    child.expect('stop reason = step')
    print("--> [PASS] Step 1/5 inside bitslice_handler successful!")

    # Step 2
    time.sleep(0.2)
    child.sendline('step')
    child.expect('stop reason = step')
    print("--> [PASS] Step 2/5 inside bitslice_handler successful!")

    # Step 3
    time.sleep(0.2)
    child.sendline('step')
    child.expect('stop reason = step')
    print("--> [PASS] Step 3/5 inside bitslice_handler successful!")

    # Step 4
    time.sleep(0.2)
    child.sendline('step')
    child.expect('stop reason = step')
    print("--> [PASS] Step 4/5 inside bitslice_handler successful!")

    # Step 5
    time.sleep(0.2)
    child.sendline('step')
    child.expect('stop reason = step')
    print("--> [PASS] Step 5/5 inside bitslice_handler successful!")

    child.sendline('quit')
    child.sendline('Y')
    child.expect(pexpect.EOF)
    print("--> [PASS] Ctrl-C consecutive stepping test completed!")
    print("--> [PASS] Ctrl-C asynchronous halt successful!")

    # ------------------------------------------------------------------------------
    # PHASE 5: CONNECTING TO AN ALREADY EXECUTING TARGET
    # ------------------------------------------------------------------------------
    print("\n=== PHASE 5: Connecting to an already executing target ===\n")
    child1 = pexpect.spawn('../../build/bin/lldb out/ezh_debug_test.elf', encoding='utf-8', timeout=15)
    child1.logfile = log_file
    child1.sendline('settings set use-color false')
    child1.expect(r"\(lldb\)")
    child1.sendline('process connect -p ezh-remote connect://localhost:3333')
    child1.expect(r"\(lldb\)")
    child1.sendline('continue')
    time.sleep(1.0)
    child1.sendline('quit')
    child1.sendline('Y')
    child1.expect(pexpect.EOF)

    # Attach to the already executing target
    child2 = pexpect.spawn('../../build/bin/lldb out/ezh_debug_test.elf', encoding='utf-8', timeout=15)
    child2.logfile = log_file
    child2.sendline('settings set use-color false')
    child2.expect(r"\(lldb\)")
    child2.sendline('process connect -p ezh-remote connect://localhost:3333')
    child2.expect('Process 1 stopped')
    child2.sendline('register read pc')
    child2.expect('pc = 0x')
    child2.sendline('quit')
    child2.sendline('Y')
    child2.expect(pexpect.EOF)
    print("--> [PASS] Re-connecting to executing target successful!\n")

    # ------------------------------------------------------------------------------
    # PHASE 6: BREAKPOINT CAPACITY AND SLOT EXHAUSTION
    # ------------------------------------------------------------------------------
    print("\n=== PHASE 6: Breakpoint Capacity & Slot Exhaustion ===\n")
    child_cap = pexpect.spawn('../../build/bin/lldb out/ezh_debug_test.elf', encoding='utf-8', timeout=15)
    child_cap.logfile = log_file

    child_cap.sendline('settings set use-color false')
    child_cap.expect(r"\(lldb\)")

    # Connect to target
    child_cap.sendline('process connect -p ezh-remote connect://localhost:3333')
    child_cap.expect('Process 1 stopped')

    # Halt EZH core (physical reset and clean start setup)
    child_cap.sendline('memory write -s 4 0x40027024 0xC0DE0000')
    child_cap.expect(r"\(lldb\)")
    child_cap.sendline('expr *(volatile int *)0x40000010 |= (1 << 30)')
    child_cap.expect(r"\(lldb\)")
    child_cap.sendline('expr *(volatile int *)0x40000010 &= ~(1 << 30)')
    child_cap.expect(r"\(lldb\)")

    # Load ELF module mappings
    child_cap.sendline('target modules load --load --file ezh_debug_test.elf --slide 0')
    child_cap.expect(r"\(lldb\)")

    # Clear variables & bitslice
    child_cap.sendline('expr exc_signal = 0')
    child_cap.expect(r"\(lldb\)")
    child_cap.sendline('memory write -s 4 0x40027048 0x00000080')
    child_cap.expect(r"\(lldb\)")

    # Halt EZH core virtually
    child_cap.sendline('memory write -s 4 0x40027024 0xC0DE0010')
    child_cap.expect(r"\(lldb\)")

    # Ignite core via breakpoint continue
    child_cap.sendline('breakpoint set -n test_1b_1c_ignition_and_corruption')
    child_cap.expect('Breakpoint 1')
    child_cap.sendline('continue')
    child_cap.expect('stop reason = breakpoint 1.1')

    # Delete the ignition breakpoint so it doesn't interfere
    child_cap.sendline('breakpoint delete 1')
    child_cap.expect('1 breakpoints deleted')

    # Perform a dummy step (si) to force cleanup of the ignition breakpoint's slot (slot 0)
    # This moves PC from 0x241007e8 to 0x241007ec, and leaves slot 0 cleanly freed!
    child_cap.sendline('si')
    child_cap.expect('stop reason = instruction step into')

    # 16 Distinct instruction addresses to set breakpoints on:
    # Current PC is 0x241007fc. Next PC is 0x24100800. Exclude both!
    addresses = [
        "0x241007d0", "0x241007d4", "0x241007d8", "0x241007dc",
        "0x241007e0", "0x241007e4", "0x241007e8", "0x241007f4",
        "0x241007f8", "0x24100804", "0x24100808", "0x2410080c",
        "0x24100810", "0x24100814", "0x2410081c", "0x24100820"
    ]

    print("[Step 1: Setting 16 software breakpoints]")
    for i, addr in enumerate(addresses):
        child_cap.sendline(f'breakpoint set -a {addr}')
        child_cap.expect(f'Breakpoint {i+2}')
    print("--> [PASS] Successfully enabled 16 software breakpoints!")

    print("[Step 2: Attempting 17th software breakpoint (Must Fail)]")
    child_cap.sendline('breakpoint set -a 0x24100818')
    child_cap.expect('only supports up to 16 active software breakpoints')
    print("--> [PASS] 17th breakpoint correctly rejected with slot capacity limit error!")

    print("[Step 3: Attempting to step while 16 slots are full (Must Fail)]")
    child_cap.sendline('si')
    child_cap.expect('All 16 software breakpoint slots are currently full')
    print("--> [PASS] Single-stepping correctly blocked with slot exhaustion error!")

    print("[Step 4: Deleting breakpoint 2 to free slot 0]")
    child_cap.sendline('breakpoint delete 2')
    child_cap.expect('1 breakpoints deleted')
    print("--> [PASS] Breakpoint 2 deleted successfully!")

    print("[Step 5: Attempting to step again (Must Succeed)]")
    child_cap.sendline('si')
    child_cap.expect('stop reason = instruction step into')
    print("--> [PASS] Single-stepping succeeded after freeing a slot!")

    child_cap.sendline('quit')
    child_cap.sendline('Y')
    child_cap.expect(pexpect.EOF)
    print("--> [PASS] Breakpoint capacity limits verification successful!\n")

    # ------------------------------------------------------------------------------
    # PHASE 7: STEPPING AND RESUMING OVER BREAKPOINTS
    # ------------------------------------------------------------------------------
    print("\n=== PHASE 7: Stepping & Continuing Over Breakpoints ===\n")
    child_bp = pexpect.spawn('../../build/bin/lldb out/ezh_debug_test.elf', encoding='utf-8', timeout=15)
    child_bp.logfile = log_file

    child_bp.sendline('settings set use-color false')
    child_bp.expect(r"\(lldb\)")

    # Connect to target
    child_bp.sendline('process connect -p ezh-remote connect://localhost:3333')
    child_bp.expect('Process 1 stopped')

    # Halt EZH core (physical reset and clean start setup)
    child_bp.sendline('memory write -s 4 0x40027024 0xC0DE0000')
    child_bp.expect(r"\(lldb\)")
    child_bp.sendline('expr *(volatile int *)0x40000010 |= (1 << 30)')
    child_bp.expect(r"\(lldb\)")
    child_bp.sendline('expr *(volatile int *)0x40000010 &= ~(1 << 30)')
    child_bp.expect(r"\(lldb\)")

    # Load ELF module mappings
    child_bp.sendline('target modules load --load --file ezh_debug_test.elf --slide 0')
    child_bp.expect(r"\(lldb\)")

    # Clear variables & bitslice
    child_bp.sendline('expr exc_signal = 0')
    child_bp.expect(r"\(lldb\)")
    child_bp.sendline('memory write -s 4 0x40027048 0x00000080')
    child_bp.expect(r"\(lldb\)")

    # Halt EZH core virtually
    child_bp.sendline('memory write -s 4 0x40027024 0xC0DE0010')
    child_bp.expect(r"\(lldb\)")

    # Set breakpoint on line 230 (exc_signal = 0x77770001)
    print("[Step 1: Setting breakpoint on line 230 (Step Over Target)]")
    child_bp.sendline('breakpoint set --file ezh_debug_test.c --line 230')
    child_bp.expect('Breakpoint 1')
    print("--> [PASS] Breakpoint on line 230 set successfully!")

    # Continue to ignite the core and run to the breakpoint
    print("[Step 2: Continuing past ignition to breakpoint on line 230]")
    child_bp.sendline('continue')
    child_bp.expect('stop reason = breakpoint 1.1')
    print("--> [PASS] Target successfully hit breakpoint on line 230!")

    # Step OVER the set breakpoint on line 230
    print("[Step 3: Stepping OVER the active breakpoint on line 230]")
    child_bp.sendline('step')
    child_bp.expect('stop reason = step')
    print("--> [PASS] Stepped off breakpoint on line 230 successfully!")

    # Verify exc_signal was updated to 0x77770001 (proving execution of the breakpoint instruction!)
    print("[Step 4: Verifying exc_signal was written during the step]")
    child_bp.sendline('expr exc_signal')
    child_bp.expect('0x77770001')
    print("--> [PASS] Verified exc_signal = 0x77770001! The instruction under the breakpoint executed perfectly during step!")

    # Set breakpoint on line 231 (exc_signal = 0x77770002) while we are currently stopped there
    print("[Step 5: Setting breakpoint on line 231 (Continue Over Target)]")
    child_bp.sendline('breakpoint set --file ezh_debug_test.c --line 231')
    child_bp.expect('Breakpoint 2')
    print("--> [PASS] Breakpoint on line 231 set successfully!")

    # Set breakpoint on line 232 (exc_signal = 0x77770003) to stop immediately after continue over line 231
    print("[Step 6: Setting breakpoint on line 232 to catch continue]")
    child_bp.sendline('breakpoint set --file ezh_debug_test.c --line 232')
    child_bp.expect('Breakpoint 3')

    # Continue over the breakpoint on line 231
    print("[Step 7: Continuing OVER the active breakpoint on line 231]")
    child_bp.sendline('continue')
    child_bp.expect('stop reason = breakpoint 3.1')
    print("--> [PASS] Successfully continued past breakpoint 2 and hit breakpoint 3 on line 232!")

    # Verify exc_signal was updated to 0x77770002 (proving execution of the breakpoint instruction during continue!)
    print("[Step 8: Verifying exc_signal was written during continue]")
    child_bp.sendline('expr exc_signal')
    child_bp.expect('0x77770002')
    print("--> [PASS] Verified exc_signal = 0x77770002! The instruction under breakpoint 2 executed perfectly during continue!")

    child_bp.sendline('quit')
    child_bp.sendline('Y')
    child_bp.expect(pexpect.EOF)
    print("--> [PASS] Breakpoint stepping and resuming verification successful!\n")

    # Restore stdout before closing the log file
    sys.stdout = sys.__stdout__
    log_file.close()

    print("================================================================================")
    print("=== ALL COMPREHENSIVE DEBUGGER SUITE TESTS PASSED                            ===")
    print("================================================================================\n")

if __name__ == '__main__':
    run_comprehensive_tests()
