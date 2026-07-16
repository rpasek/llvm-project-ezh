; RUN: llc -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O3 < %s | FileCheck %s

; A return-position libcall whose result feeds only the return is in tail
; position (isUsedByReturnOnly): it forwards straight to the helper with no
; frame or RA save, instead of gosub + pushd/popd.

define i32 @divi(i32 %a, i32 %b) {
; CHECK-LABEL: divi:
; CHECK-NOT:   pushd
; CHECK:       goto __divsi3
; CHECK-NOT:   gosub
  %r = sdiv i32 %a, %b
  ret i32 %r
}

define i32 @modi(i32 %a, i32 %b) {
; CHECK-LABEL: modi:
; CHECK:       goto __modsi3
; CHECK-NOT:   gosub
  %r = srem i32 %a, %b
  ret i32 %r
}

; Not tail position -- the call result is used before the return -- so it
; stays a normal call.
define i32 @not_tail(i32 %a, i32 %b) {
; CHECK-LABEL: not_tail:
; CHECK:       gosub __divsi3
  %q = sdiv i32 %a, %b
  %r = add i32 %q, 1
  ret i32 %r
}
