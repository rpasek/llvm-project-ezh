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

import os
import subprocess
import sys
import time
import pexpect


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


def reset_target_state(lldb, elf_path='out/ezh_test.elf'):
    # Toggle SMARTDMA reset via atomic SET/CLR registers
    lldb.sendline('memory write -s 4 0x40000040 0x40000000')
    lldb.expect(r'\(lldb\)')
    lldb.sendline('memory write -s 4 0x40000070 0x40000000')
    lldb.expect(r'\(lldb\)')
    # Load ELF sections
    lldb.sendline(
        'target modules load --load --file '
        f'{os.path.basename(elf_path)} --slide 0'
    )
    lldb.expect(r'\(lldb\)')
    # Initialize exc_signal to 0xFFFFFFFF
    lldb.sendline('expr exc_signal = 0xFFFFFFFF')
    lldb.expect(r'\(lldb\)')
    # Clear bit slice
    lldb.sendline('memory write -s 4 0x40027048 0x00000080')
    lldb.expect(r'\(lldb\)')
    # Configure EZH CTRL to "initialized but halted" state (Start = 0, GPISYNCH
    # = 1)
    lldb.sendline('memory write -s 4 0x40027024 0xC0DE0010')
    lldb.expect(r'\(lldb\)')


def setup_and_connect_target(lldb, elf_path='out/ezh_test.elf'):
    lldb.sendline('settings set use-color false')
    lldb.expect(r'\(lldb\)')
    lldb.sendline('process connect -p ezh-remote connect://localhost:3333')
    lldb.expect('Process 1 stopped')
    reset_target_state(lldb, elf_path)


def run_comprehensive_tests():
    os.makedirs('out', exist_ok=True)
    log_file = open('out/debug_test.log', 'w')

    # Redirect sys.stdout to TeeLogger to write to both console and file!
    sys.stdout = TeeLogger(log_file, sys.stdout)

    print('=' * 19 + ' EZH DEBUGGER VERIFICATION SUITE ' + '=' * 20)
    print(('=' * 72) + '\n')

    # Clean up any leftover debugger processes before test suite start
    subprocess.run(['killall', '-9', 'lldb'], capture_output=True)

    # Launch LLVM debugger interactively!
    lldb = pexpect.spawn(
        '../../build/bin/lldb out/ezh_test.elf', encoding='utf-8', timeout=15
    )
    # Redirect lldb log output to the log file in real-time!
    lldb.logfile = log_file

    setup_and_connect_target(lldb)

    # Test 1A: Ignite core directly from reset by stepping (stepi) -> Must block
    # it!
    print('\n[Test 1A: Cold-Stepping Safety Protection]')
    lldb.sendline('si')
    lldb.expect('Cannot step, stepi, next or finish a non running core')
    print(
        '--> [PASS] Stepping from reset correctly blocked with safety message!'
    )

    # Test 1B: Ignite core by continue to breakpoint (tests automatic step-plan
    # cleanup!)
    print('\n[Test 1B: Target Ignition via Breakpoint Continue]')
    lldb.sendline('b test_1b_1c_ignition_and_corruption')
    lldb.expect('Breakpoint 1')
    lldb.sendline('continue')
    lldb.expect('stop reason = breakpoint 1.1')
    lldb.sendline('breakpoint disable')
    lldb.expect(r'\(lldb\)')
    print('--> [PASS] Halted target ignited successfully via continue!')

    # Test 1C: Step over register immediate loads and verify correct register
    # reads
    print('\n[Test 1C: Step Over Register Immediate Loads]')
    # Step 9 times sequentially over immediate loads (r0-r6, gpo, gpd)
    for _ in range(9):
        lldb.sendline('si')
        lldb.expect('stop reason = instruction step into')
    lldb.sendline('register read r0 r1 r2 r3 r4 r5 r6 gpo gpd')
    lldb.expect('r0 = 0x00000011')
    lldb.expect('r1 = 0x00000022')
    lldb.expect('r2 = 0x00000033')
    lldb.expect('r3 = 0x00000044')
    lldb.expect('r4 = 0x00000055')
    lldb.expect('r5 = 0x00000066')
    lldb.expect('r6 = 0x00000077')
    lldb.expect('gpo = 0x00000099')
    lldb.expect('gpd = 0x000000aa')
    print(
        '--> [PASS] Register modification tracking via stepi & register read '
        'successful!'
    )

    # ------------------------------------------------------------------------
    # PHASE 3: SUBROUTINE FLOW CONTROL (STEP, NEXT, FINISH)
    # ------------------------------------------------------------------------
    print('\n=== PHASE 3: Subroutine Flow Control (step, next, finish) ===\n')

    lldb.sendline('b test_3_subroutine_step')
    lldb.expect('Breakpoint 2')
    lldb.sendline('continue')
    lldb.expect('stop reason = breakpoint 2.1')
    lldb.sendline('breakpoint disable')
    lldb.expect(r'\(lldb\)')

    # Step 1: step over function-entry NOPs to exc_signal line (Line 20)
    time.sleep(0.2)
    lldb.sendline('step')
    lldb.expect('stop reason = step')
    lldb.expect(r'-> 38')

    # Step 2: step over exc_signal to test_3_subroutine_step_target() call (Line
    # 39)
    time.sleep(0.2)
    lldb.sendline('step')
    lldb.expect('stop reason = step')
    lldb.expect(r'-> 39')

    # VERIFY: exc_signal was updated to 0x88888888 in EZH RAM!
    lldb.sendline('print/x exc_signal')
    lldb.expect('0x88888888')
    print(
        '--> [PASS] C-level stepping over assignment correctly updated '
        'exc_signal to 0x88888888!'
    )

    # Step 3: step directly into subroutine test_3_subroutine_step_target()
    time.sleep(0.2)
    lldb.sendline('step')
    lldb.expect('stop reason = step')
    lldb.expect(
        r'frame #0: 0x[0-9a-f]+ ezh_test.elf`test_3_subroutine_step_target'
    )
    lldb.expect(r'-> 28')

    # Step 3.1: step over exc_signal assignment inside subroutine to closing
    # brace (line 29)
    time.sleep(0.2)
    lldb.sendline('step')
    lldb.expect('stop reason = step')
    lldb.expect(r'-> 29\s+\}')

    # VERIFY: exc_signal inside subroutine was updated to 0x99999999 in EZH RAM!
    lldb.sendline('print/x exc_signal')
    lldb.expect('0x99999999')
    print(
        '--> [PASS] Stepping inside subroutine correctly updated exc_signal '
        'to 0x99999999!'
    )

    # Finish out of test_3_subroutine_step_target to closing brace of caller
    # (Line 40)
    time.sleep(0.2)
    lldb.sendline('finish')
    lldb.expect('stop reason = step out')
    lldb.expect(r'-> 40')

    # Step over next statement to exit
    time.sleep(0.2)
    lldb.sendline('next')
    lldb.expect('stop reason = step over')

    # ------------------------------------------------------------------------
    # PHASE 3B: CONSECUTIVE STEPPING INSIDE CLEAN C CONDITIONAL BRANCHES
    # ------------------------------------------------------------------------
    print(
        '\n=== PHASE 3B: Consecutive Stepping inside Clean C Conditional '
        'Branches ===\n'
    )

    lldb.sendline('b test_4_c_stepping')
    lldb.expect('Breakpoint 3')
    lldb.sendline('continue')
    lldb.expect('stop reason = breakpoint 3.1')
    lldb.sendline('breakpoint disable')
    lldb.expect(r'\(lldb\)')

    # Step 1: step over function-entry synchronization NOPs to variable
    # declaration
    time.sleep(0.2)
    lldb.sendline('step')
    lldb.expect('stop reason = step')
    lldb.expect('ezh_test.elf`test_4_c_stepping')

    # Step 2: step over variable declaration to the first conditional branch (x
    # == 1)
    time.sleep(0.2)
    lldb.sendline('step')
    lldb.expect('stop reason = step')
    lldb.expect(r'if \(x == 1\)')

    # Perform 6 consecutive steps to step through both Taken and Not Taken
    # conditional branches!
    print(
        '\n[Test 3B: Consecutive Stepping inside C Conditional Branches (6 '
        'times)]'
    )

    # Step 3 (x == 1 Taken): should step cleanly into the True block!
    time.sleep(0.2)
    lldb.sendline('step')
    lldb.expect('stop reason = step')
    lldb.expect(r'-> 210\s+exc_signal = 0xaaaa0001')
    print('--> [PASS] Step 1/6 (ZE=1 Taken Branch into True block) successful!')

    # Step 4 (Block exit): steps from exc_signal to the } else { block exit line
    time.sleep(0.2)
    lldb.sendline('step')
    lldb.expect('stop reason = step')

    # VERIFY: exc_signal in Taken block was written successfully!
    lldb.sendline('print/x exc_signal')
    lldb.expect('0xaaaa0001')
    print(
        '--> [PASS] C-level conditional step correctly wrote exc_signal to '
        '0xaaaa0001!'
    )

    # Step 5 (Next Branch entry): steps from } else { to the next conditional
    # branch (x == 2)
    time.sleep(0.2)
    lldb.sendline('step')
    lldb.expect('stop reason = step')
    lldb.expect(r'if \(x == 2\)')
    print('--> [PASS] Step 2/6 (Exit Taken Block to next branch) successful!')

    # Step 6 (x == 2 Not Taken): should step cleanly bypassing True block to the
    # False block!
    time.sleep(0.2)
    lldb.sendline('step')
    lldb.expect('stop reason = step')
    lldb.expect(r'-> 218\s+exc_signal = 0xaaaa0004')
    print(
        '--> [PASS] Step 3/6 (ZE=0 Not Taken Branch into False block) '
        'successful!'
    )

    # Step 7 (Exit False Block): steps to the exit of the function }
    time.sleep(0.2)
    lldb.sendline('step')
    lldb.expect('stop reason = step')
    lldb.expect(r'-> 220\s+\}')

    # VERIFY: exc_signal in False block was written successfully!
    lldb.sendline('print/x exc_signal')
    lldb.expect('0xaaaa0004')
    print(
        '--> [PASS] C-level conditional step correctly wrote exc_signal to '
        '0xaaaa0004!'
    )
    print('--> [PASS] Step 4/6 (Exit False Block) successful!')

    print(
        '--> [PASS] Subroutine Flow Control and C Conditional stepping '
        'Successful!'
    )

    # ------------------------------------------------------------------------
    # PHASE 4: ASYNCHRONOUS INTERRUPT (CTRL-C)
    # ------------------------------------------------------------------------
    print('\n=== PHASE 4: Asynchronous Interrupt (Ctrl-C) ===\n')
    reset_target_state(lldb)
    lldb.sendline('expr *(volatile int *)0x40027034 = (int)&debug_handler')
    lldb.expect(r'\(lldb\)')

    # Run EZH target cleanly
    lldb.sendline('continue')
    time.sleep(1.5)

    # Send Ctrl-C halt request
    lldb.sendcontrol('c')
    lldb.expect('Process 1 stopped')

    print(
        '\n[Test 4: Consecutive Stepping (5 times) inside bitslice_handler '
        'after Ctrl-C]'
    )

    # Step 1
    time.sleep(0.2)
    lldb.sendline('step')
    lldb.expect('stop reason = step')
    print('--> [PASS] Step 1/5 inside bitslice_handler successful!')

    # Step 2
    time.sleep(0.2)
    lldb.sendline('step')
    lldb.expect('stop reason = step')
    print('--> [PASS] Step 2/5 inside bitslice_handler successful!')

    # Step 3
    time.sleep(0.2)
    lldb.sendline('step')
    lldb.expect('stop reason = step')
    print('--> [PASS] Step 3/5 inside bitslice_handler successful!')

    # Step 4
    time.sleep(0.2)
    lldb.sendline('step')
    lldb.expect('stop reason = step')
    print('--> [PASS] Step 4/5 inside bitslice_handler successful!')

    # Step 5
    time.sleep(0.2)
    lldb.sendline('step')
    lldb.expect('stop reason = step')
    print('--> [PASS] Step 5/5 inside bitslice_handler successful!')

    print('--> [PASS] Ctrl-C consecutive stepping test completed!')
    print('--> [PASS] Ctrl-C asynchronous halt successful!')

    # ------------------------------------------------------------------------
    # PHASE 5: BREAKPOINT CAPACITY AND SLOT EXHAUSTION
    # ------------------------------------------------------------------------
    print('\n=== PHASE 5: Breakpoint Capacity & Slot Exhaustion ===\n')
    reset_target_state(lldb)

    # Ignite core via breakpoint continue
    lldb.sendline('breakpoint set -n test_1b_1c_ignition_and_corruption')
    lldb.expect(r'Breakpoint \d+')
    lldb.sendline('continue')
    lldb.expect(r'stop reason = breakpoint \d+\.1')

    # Delete the ignition breakpoint so it doesn't interfere
    lldb.sendline('breakpoint delete -f')
    lldb.expect('All breakpoints removed')

    # Perform a dummy step (si) to force cleanup of the ignition breakpoint's
    # slot (slot 0)
    # This moves PC from 0x241007e8 to 0x241007ec, and leaves slot 0 cleanly
    # freed!
    lldb.sendline('si')
    lldb.expect('stop reason = instruction step into')

    # 16 Distinct instruction addresses to set breakpoints on:
    # Current PC is 0x100534. Next PC is 0x100538. Exclude both!
    addresses = [
        '0x1005f8',
        '0x1005fc',
        '0x100600',
        '0x100604',
        '0x100608',
        '0x10060c',
        '0x100610',
        '0x100614',
        '0x100618',
        '0x10061c',
        '0x100620',
        '0x100624',
        '0x100628',
        '0x10062c',
        '0x100630',
        '0x100634',
    ]

    print('[Step 1: Setting 16 software breakpoints]')
    first_bp_id = None
    for i, addr in enumerate(addresses):
        lldb.sendline(f'breakpoint set -a {addr}')
        idx = lldb.expect(r'Breakpoint (\d+)')
        if i == 0:
            first_bp_id = lldb.match.group(1)
    print('--> [PASS] Successfully enabled 16 software breakpoints!')

    print('[Step 2: Attempting 17th software breakpoint (Must Fail)]')
    lldb.sendline('breakpoint set -a 0x100638')
    lldb.expect('No available software breakpoint slots in range of address')
    print(
        '--> [PASS] 17th breakpoint correctly rejected with slot capacity '
        'limit error!'
    )

    print('[Step 3: Attempting to step while 16 slots are full (Must Fail)]')
    lldb.sendline('si')
    lldb.expect(
        'Cannot single-step EZH target: No available software breakpoint '
        'slots in range of next PC'
    )
    print(
        '--> [PASS] Single-stepping correctly blocked with slot exhaustion '
        'error!'
    )

    print(f'[Step 4: Deleting breakpoint {first_bp_id} to free slot 0]')
    lldb.sendline(f'breakpoint delete {first_bp_id}')
    lldb.expect('1 breakpoints deleted')
    print(f'--> [PASS] Breakpoint {first_bp_id} deleted successfully!')

    print('[Step 5: Attempting to step again (Must Succeed)]')
    lldb.sendline('si')
    lldb.expect('stop reason = instruction step into')
    print('--> [PASS] Single-stepping succeeded after freeing a slot!')

    # Clean up all breakpoints before next phase
    lldb.sendline('breakpoint delete -f')
    lldb.expect('All breakpoints removed')
    print('--> [PASS] Breakpoint capacity limits verification successful!\n')

    # ------------------------------------------------------------------------
    # PHASE 6: MULTIPLE SIMULTANEOUS BREAKPOINTS VERIFICATION
    # ------------------------------------------------------------------------
    print('\n=== PHASE 6: Multiple Simultaneous Breakpoints ===\n')
    reset_target_state(lldb)

    # Set 3 breakpoints simultaneously in test_7_multi_breakpoints
    print('[Step 1: Setting 3 breakpoints in test_7_multi_breakpoints]')
    lldb.sendline('breakpoint set --file ezh_debug_test.c --line 231')
    lldb.expect(r'Breakpoint \d+')
    lldb.sendline('breakpoint set --file ezh_debug_test.c --line 232')
    lldb.expect(r'Breakpoint \d+')
    lldb.sendline('breakpoint set --file ezh_debug_test.c --line 233')
    lldb.expect(r'Breakpoint \d+')
    print('--> [PASS] 3 breakpoints set successfully!')

    # Continue to hit Breakpoint 1
    print('[Step 2: Continuing to Breakpoint 1]')
    lldb.sendline('continue')
    lldb.expect(r'stop reason = breakpoint \d+\.1')
    print('--> [PASS] Hit Breakpoint 1 successfully!')

    # Verify a is still 0 (before assignment)
    print('[Step 3: Verifying variable state at Breakpoint 1]')
    lldb.sendline('expr a')
    lldb.expect('0')
    print('--> [PASS] Verified a = 0 at Breakpoint 1!')

    # Continue to hit Breakpoint 2
    print('[Step 4: Continuing to Breakpoint 2]')
    lldb.sendline('continue')
    lldb.expect(r'stop reason = breakpoint \d+\.1')
    print('--> [PASS] Hit Breakpoint 2 successfully!')

    # Verify a is 10 (instruction under BP 1 executed!) and b is still 0
    print('[Step 5: Verifying variable state at Breakpoint 2]')
    lldb.sendline('expr a')
    lldb.expect('10')
    lldb.sendline('expr b')
    lldb.expect('0')
    print('--> [PASS] Verified a = 10 and b = 0 at Breakpoint 2!')

    # Continue to hit Breakpoint 3
    print('[Step 6: Continuing to Breakpoint 3]')
    lldb.sendline('continue')
    lldb.expect(r'stop reason = breakpoint \d+\.1')
    print('--> [PASS] Hit Breakpoint 3 successfully!')

    # Verify a is 10, b is 20 (instruction under BP 2 executed!) and c is still
    # 0
    print('[Step 7: Verifying variable state at Breakpoint 3]')
    lldb.sendline('expr a')
    lldb.expect('10')
    lldb.sendline('expr b')
    lldb.expect('20')
    lldb.sendline('expr c')
    lldb.expect('0')
    print('--> [PASS] Verified a = 10, b = 20, and c = 0 at Breakpoint 3!')

    # Continue to finish the function and halt core
    print('[Step 8: Continuing to complete function and halting]')
    lldb.sendline('continue')
    time.sleep(0.5)
    lldb.sendcontrol('c')
    lldb.expect('Process 1 stopped')

    # Clean up breakpoints before next phase
    lldb.sendline('breakpoint delete -f')
    lldb.expect('All breakpoints removed')
    print('--> [PASS] Multiple breakpoints verification successful!\n')

    # ------------------------------------------------------------------------
    # PHASE 7: STEPPING AND RESUMING OVER BREAKPOINTS
    # ------------------------------------------------------------------------
    print('\n=== PHASE 7: Stepping & Continuing Over Breakpoints ===\n')
    reset_target_state(lldb)

    # Set breakpoint on line 247 (exc_signal = 0x77770001)
    print('[Step 1: Setting breakpoint on line 247 (Step Over Target)]')
    lldb.sendline('breakpoint set --file ezh_debug_test.c --line 247')
    lldb.expect(r'Breakpoint \d+')
    print('--> [PASS] Breakpoint on line 247 set successfully!')

    # Continue to ignite the core and run to the breakpoint
    print('[Step 2: Continuing past ignition to breakpoint on line 247]')
    lldb.sendline('continue')
    lldb.expect(r'stop reason = breakpoint \d+\.1')
    print('--> [PASS] Target successfully hit breakpoint on line 247!')

    # Step OVER the set breakpoint on line 247
    print('[Step 3: Stepping OVER the active breakpoint on line 247]')
    lldb.sendline('step')
    lldb.expect('stop reason = step')
    print('--> [PASS] Stepped off breakpoint on line 247 successfully!')

    # Verify exc_signal was updated to 0x77770001 (proving execution of the
    # breakpoint instruction!)
    print('[Step 4: Verifying exc_signal was written during the step]')
    lldb.sendline('expr exc_signal')
    lldb.expect('0x77770001')
    print(
        '--> [PASS] Verified exc_signal = 0x77770001! The instruction under '
        'the breakpoint executed perfectly during step!'
    )

    # Set breakpoint on line 248 (exc_signal = 0x77770002) while we are
    # currently stopped there
    print('[Step 5: Setting breakpoint on line 248 (Continue Over Target)]')
    lldb.sendline('breakpoint set --file ezh_debug_test.c --line 248')
    lldb.expect(r'Breakpoint \d+')
    print('--> [PASS] Breakpoint on line 248 set successfully!')

    # Set breakpoint on line 249 (exc_signal = 0x77770003) to stop immediately
    # after continue over line 248
    print('[Step 6: Setting breakpoint on line 249 to catch continue]')
    lldb.sendline('breakpoint set --file ezh_debug_test.c --line 249')
    lldb.expect(r'Breakpoint \d+')
    print('--> [PASS] Breakpoint on line 249 set successfully!')

    # Continue over the breakpoint on line 248
    print('[Step 7: Continuing OVER the active breakpoint on line 248]')
    lldb.sendline('continue')
    lldb.expect(r'stop reason = breakpoint \d+\.1')
    print(
        '--> [PASS] Successfully continued past breakpoint 2 and hit '
        'breakpoint 3 on line 249!'
    )

    # Verify exc_signal was updated to 0x77770002 (proving execution of the
    # breakpoint instruction during continue!)
    print('[Step 8: Verifying exc_signal was written during continue]')
    lldb.sendline('expr exc_signal')
    lldb.expect('0x77770002')
    print(
        '--> [PASS] Verified exc_signal = 0x77770002! The instruction under '
        'breakpoint 2 executed perfectly during continue!'
    )

    # Clean up breakpoints before next phase
    lldb.sendline('breakpoint delete -f')
    lldb.expect('All breakpoints removed')
    print(
        '--> [PASS] Breakpoint stepping and resuming verification successful!\n'
    )

    # ------------------------------------------------------------------------
    # PHASE 8: ABI FUNCTION CALLING & RETURN VALUE EXTRACTION (call, finish)
    # ------------------------------------------------------------------------
    print('\n=== PHASE 8: ABI Function Calling & Return Value Extraction ===\n')
    reset_target_state(lldb)

    # Set breakpoint on test_abi_add
    print('[Step 1: Setting breakpoint on test_abi_add]')
    lldb.sendline('breakpoint set -n test_abi_add')
    lldb.expect(r'Breakpoint \d+')
    print('--> [PASS] Breakpoint on test_abi_add set successfully!')

    # Continue to test_abi_add
    print('[Step 2: Continuing to test_abi_add breakpoint]')
    lldb.sendline('continue')
    lldb.expect(r'stop reason = breakpoint \d+\.1')
    print('--> [PASS] Hit test_abi_add breakpoint successfully!')

    # Finish function and verify return value object extraction!
    print('[Step 3: Finishing function and verifying return value extraction]')
    lldb.sendline('finish')
    lldb.expect('stop reason = step out')
    lldb.expect(r'Return value: \(int\) \$[0-9]+ = 100')
    print(
        '--> [PASS] Return value object ($3 = 100) extracted successfully by '
        'ABIEZH::GetReturnValueObjectImpl!'
    )

    # Delete breakpoint so it doesn't interfere with expression evaluation
    lldb.sendline('breakpoint delete -f')
    lldb.expect('All breakpoints removed')

    # Now let's test calling a function directly using expr / call!
    print(
        '[Step 4: Executing function call expression on hardware (call '
        'test_abi_add(100, 200, 300, 400))]'
    )
    lldb.sendline('expr test_abi_add(100, 200, 300, 400)')
    lldb.expect(r'\(int\) \$[0-9]+ = 1000')
    print(
        '--> [PASS] Function call expression evaluated on target hardware ($4 '
        '= 1000) successfully by ABIEZH::PrepareTrivialCall!'
    )

    # ------------------------------------------------------------------------
    # PHASE 9: CONNECTING TO AN ALREADY EXECUTING TARGET
    # ------------------------------------------------------------------------
    print('\n=== PHASE 9: Connecting to an already executing target ===\n')
    lldb.sendline('continue')
    time.sleep(1.0)
    lldb.sendline('quit')
    lldb.sendline('Y')
    lldb.expect(pexpect.EOF)

    # Attach to the already executing target without resetting hardware!
    lldb_reconnect = pexpect.spawn(
        '../../build/bin/lldb out/ezh_test.elf', encoding='utf-8', timeout=15
    )
    lldb_reconnect.logfile = log_file
    lldb_reconnect.sendline('settings set use-color false')
    lldb_reconnect.expect(r'\(lldb\)')
    lldb_reconnect.sendline(
        'process connect -p ezh-remote connect://localhost:3333'
    )
    lldb_reconnect.expect('Process 1 stopped')
    lldb_reconnect.sendline('register read pc')
    lldb_reconnect.expect('pc = 0x')
    lldb_reconnect.sendline('quit')
    lldb_reconnect.sendline('Y')
    lldb_reconnect.expect(pexpect.EOF)
    print('--> [PASS] Re-connecting to executing target successful!\n')

    # Restore stdout before closing the log file
    sys.stdout = sys.__stdout__
    log_file.close()

    print(('=' * 72) + '\n')
    print('=' * 27 + ' ALL TESTS PASSED ' + '=' * 27)
    print(('=' * 72) + '\n')


if __name__ == '__main__':
    run_comprehensive_tests()
