# RUN: not llvm-mc -triple=ezh %s 2>&1 | FileCheck %s
#
# NXP-confirmed operand restrictions: the register field at bits[27:24]
# accepts only R0-R7 on silicon for these three instruction families --
# 0x1D register-offset load/store (the index register), 0x19
# register-controlled shifts (the shift-amount register), and 0x16 andor
# (the rotate-amount register). The assembler must reject anything else
# instead of silently mis-encoding it.

ldr_reg r0, r1, ra
# CHECK: [[@LINE-1]]:17: error: invalid operand for instruction
ldr_regb r0, r1, sp
# CHECK: [[@LINE-1]]:18: error: invalid operand for instruction
ldr_regbs r0, r1, gpo
# CHECK: [[@LINE-1]]:19: error: invalid operand for instruction
str_reg r1, r0, gpd
# CHECK: [[@LINE-1]]:17: error: invalid operand for instruction
str_regb r1, r0, gpi
# CHECK: [[@LINE-1]]:18: error: invalid operand for instruction

rlsl r0, r1, ra
# CHECK: [[@LINE-1]]:14: error: invalid operand for instruction
rlsr r0, r1, cfm
# CHECK: [[@LINE-1]]:14: error: invalid operand for instruction

andor r0, r1, r2, gpo
# CHECK: [[@LINE-1]]:19: error: invalid operand for instruction
andors r0, r1, r2, pc
# CHECK: [[@LINE-1]]:20: error: invalid operand for instruction
