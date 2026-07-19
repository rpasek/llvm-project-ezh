# load_imm has only an 11-bit immediate field, so it cannot carry a 32-bit
# symbol/relocation value. The assembler must diagnose this rather than silently
# emitting a FIXUP_EZH_32 that clobbers the entire instruction word at link time
# (R_EZH_32 resolves via a plain write32le). The supported ways to put a symbol
# address into an instruction are a PC-relative literal-pool load (ldr rX, pc,
# <off>) or the load_simm/or_imm pair.
#
# RUN: not llvm-mc -triple ezh-none-elf --filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

# CHECK: error: symbol/relocation operand does not fit this instruction's immediate field
    load_imm r0, some_external_symbol

# per_read/per_write route an expr operand through getMachineOpValue too, so a
# symbol there is diagnosed the same way.
# CHECK: error: symbol/relocation operand does not fit this instruction's immediate field
    per_read r0, some_external_symbol

# The hi/lo materialization pair, by contrast, may carry a symbol -- no error.
# CHECK-NOT: error
    load_simm r1, some_external_symbol
    or_imm r1, r1, some_external_symbol
