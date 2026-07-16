; RUN: llc -verify-machineinstrs -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O3 < %s | FileCheck %s

; Variable 64-bit shifts inline as the branchless two-word parts sequence
; instead of calling __ashldi3 / __lshrdi3 / __ashrdi3.

define i64 @shl64(i64 %v, i32 %n) {
; CHECK-LABEL: shl64:
; CHECK-NOT:   gosub
; CHECK:       rlsl
; CHECK-NOT:   gosub
entry:
  %nn = zext i32 %n to i64
  %r = shl i64 %v, %nn
  ret i64 %r
}

define i64 @lshr64(i64 %v, i32 %n) {
; CHECK-LABEL: lshr64:
; CHECK-NOT:   gosub
; CHECK:       rlsr
entry:
  %nn = zext i32 %n to i64
  %r = lshr i64 %v, %nn
  ret i64 %r
}

define i64 @ashr64(i64 %v, i32 %n) {
; CHECK-LABEL: ashr64:
; CHECK-NOT:   gosub
; CHECK:       rasr
entry:
  %nn = zext i32 %n to i64
  %r = ashr i64 %v, %nn
  ret i64 %r
}

; Constant-amount shifts were already inline (no parts, no libcall).
define i64 @shlc(i64 %v) {
; CHECK-LABEL: shlc:
; CHECK-NOT:   gosub
; CHECK-NOT:   rlsl
entry:
  %r = shl i64 %v, 5
  ret i64 %r
}
