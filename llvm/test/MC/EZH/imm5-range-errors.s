# imm5 is a 5-bit field shared by the bit ops (bset/bclr/btog/btst_imm) and the
# shifted-ALU shift amount. An out-of-range value -- including a negative literal
# that would two's-complement-wrap to the wrong bit -- must be diagnosed at
# encode time, not silently truncated to 5 bits.
#
# RUN: not llvm-mc -triple ezh-none-elf --filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

# CHECK: error: bit position / shift amount 40 is out of range
    bset_imm r0, r1, 40
# CHECK: error: bit position / shift amount 32 is out of range
    add_lsl r0, r1, r2, 32
# CHECK: error: bit position / shift amount -1 is out of range
    btst_imm r0, r1, -1
