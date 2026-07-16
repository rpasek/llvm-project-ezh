; RUN: llc -verify-machineinstrs -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O3 < %s | FileCheck %s

; Signed comparisons bias both operands with btog_imm bit 31 to reuse the
; unsigned compare. A constant operand absorbs the bias at compile time
; (1000000 ^ 0x80000000 = 0x800f4240 below), so only the variable side pays
; for a btog_imm -- even when the constant was already custom-lowered to a
; pool load.
define i32 @cmp_big_const(i32 %x) {
; CHECK-LABEL: cmp_big_const:
; CHECK:       btog_imm
; CHECK-NOT:   btog_imm
; CHECK:       .long 2148483648
  %c = icmp sgt i32 %x, 1000000
  %r = zext i1 %c to i32
  ret i32 %r
}

; Booleans materialize as exactly 0 or 1, so a re-mask with and_imm 1 must
; fold away (ZeroOrOneBooleanContent).
define i32 @bool_no_mask(i32 %a, i32 %b) {
; CHECK-LABEL: bool_no_mask:
; CHECK-NOT:   and_imm
; CHECK:       load_imm_ca r{{[0-9]}}, 1
; CHECK-NOT:   and_imm
  %c = icmp ugt i32 %a, %b
  %z = zext i1 %c to i32
  %r = and i32 %z, 1
  ret i32 %r
}
