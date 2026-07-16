; RUN: llc -verify-machineinstrs -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O3 < %s | FileCheck %s

; Ordered 64-bit compares use the subtract-with-borrow idiom through the
; SETCCCARRY expansion (subs lo; sbcs hi; carry test) instead of the
; generic high/low select chain. Signed conditions bias the sign bit of
; both high words first, since EZH has no overflow flag.

define i32 @ult64(i64 %a, i64 %b) {
; CHECK-LABEL: ult64:
; CHECK:       subs
; CHECK-NEXT:  sbcs
; CHECK-NEXT:  load_imm_ca r0, 1
  %c = icmp ult i64 %a, %b
  %r = zext i1 %c to i32
  ret i32 %r
}

define i32 @slt64(i64 %a, i64 %b) {
; CHECK-LABEL: slt64:
; CHECK:       btog_imm r{{[0-9]}}, r{{[0-9]}}, 31
; CHECK:       btog_imm r{{[0-9]}}, r{{[0-9]}}, 31
; CHECK:       subs
; CHECK-NEXT:  sbcs
; CHECK-NEXT:  load_imm_ca r0, 1
  %c = icmp slt i64 %a, %b
  %r = zext i1 %c to i32
  ret i32 %r
}

; Flipped conditions come through the same path with swapped operands.
define i32 @ugt64(i64 %a, i64 %b) {
; CHECK-LABEL: ugt64:
; CHECK:       subs
; CHECK-NEXT:  sbcs
; CHECK-NEXT:  load_imm_ca r0, 1
  %c = icmp ugt i64 %a, %b
  %r = zext i1 %c to i32
  ret i32 %r
}

define i32 @sge64(i64 %a, i64 %b) {
; CHECK-LABEL: sge64:
; CHECK:       subs
; CHECK-NEXT:  sbcs
; CHECK-NEXT:  load_imm_nc r0, 1
  %c = icmp sge i64 %a, %b
  %r = zext i1 %c to i32
  ret i32 %r
}

declare void @g()

define void @br64(i64 %a, i64 %b) {
; CHECK-LABEL: br64:
; CHECK:       subs
; CHECK-NEXT:  sbcs
; CHECK-NOT:   gosub __
entry:
  %c = icmp ult i64 %a, %b
  br i1 %c, label %t, label %f
t:
  call void @g()
  br label %f
f:
  ret void
}
