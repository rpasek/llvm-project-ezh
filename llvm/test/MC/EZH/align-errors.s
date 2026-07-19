# Branch/call/word-offset targets are encoded as (value >> 2), so a constant
# target must be 4-byte aligned. These were assert()s, which vanish in a release
# (NDEBUG) build and silently truncate the low bits; they are reported as MC
# diagnostics instead.
#
# RUN: not llvm-mc -triple ezh-none-elf --filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

# CHECK: error: word offset must be 4-byte aligned
    ldr r0, pc, 2
# CHECK: error: branch target must be 4-byte aligned
    goto 0x2
# CHECK: error: call target must be 4-byte aligned
    gosub 0x2
