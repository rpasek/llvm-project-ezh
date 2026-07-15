; RUN: llc -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O3 < %s | FileCheck %s

; Signed compares against zero subtract zero -- no overflow is possible, so
; the sign-derived conditions are exact and the whole bias sequence
; (btog_imm + INT_MIN materialization + carry test) disappears. The +/-1
; bounds canonicalize into the zero bound first.

define i32 @lt0(i32 %x, i32 %a, i32 %b) {
; CHECK-LABEL: lt0:
; CHECK-NOT:   btog_imm
; CHECK:       sub_imms r{{[0-9]}}, r0, 0
; CHECK-NOT:   btog_imm
  %c = icmp slt i32 %x, 0
  %r = select i1 %c, i32 %a, i32 %b
  ret i32 %r
}

; x < 1 becomes x <= 0 (ZB condition), still bias-free.
define i32 @lt1(i32 %x, i32 %a, i32 %b) {
; CHECK-LABEL: lt1:
; CHECK-NOT:   btog_imm
; CHECK:       sub_imms r{{[0-9]}}, r0, 0
; CHECK-NOT:   btog_imm
  %c = icmp slt i32 %x, 1
  %r = select i1 %c, i32 %a, i32 %b
  ret i32 %r
}

; x > -1 becomes x >= 0 (PO condition).
define i32 @gtm1(i32 %x, i32 %a, i32 %b) {
; CHECK-LABEL: gtm1:
; CHECK-NOT:   btog_imm
; CHECK:       sub_imms r{{[0-9]}}, r0, 0
; CHECK-NOT:   btog_imm
  %c = icmp sgt i32 %x, -1
  %r = select i1 %c, i32 %a, i32 %b
  ret i32 %r
}

; A constant on the left swaps over: 0 < x is x > 0.
define i32 @zero_lt(i32 %x, i32 %a, i32 %b) {
; CHECK-LABEL: zero_lt:
; CHECK-NOT:   btog_imm
; CHECK:       sub_imms r{{[0-9]}}, r0, 0
; CHECK-NOT:   btog_imm
  %c = icmp slt i32 0, %x
  %r = select i1 %c, i32 %a, i32 %b
  ret i32 %r
}

; Compares against other constants still need the bias (pre-biased at
; compile time on the constant side).
define i32 @gt5(i32 %x, i32 %a, i32 %b) {
; CHECK-LABEL: gt5:
; CHECK:       btog_imm
  %c = icmp sgt i32 %x, 5
  %r = select i1 %c, i32 %a, i32 %b
  ret i32 %r
}
