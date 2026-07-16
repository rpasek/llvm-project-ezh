; RUN: llc -verify-machineinstrs -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O3 < %s | FileCheck %s

; 64-bit add/sub expand to the native carry chain (adds/adc, subs/sbc)
; instead of the generic boolean expansion: two instructions per
; operation. The glue between the pair keeps them adjacent, so nothing
; that could touch the flags is ever scheduled in between.

define i64 @add64(i64 %a, i64 %b) {
; CHECK-LABEL: add64:
; CHECK:       adds r0, r{{[0-9]}}, r{{[0-9]}}
; CHECK-NEXT:  adc r1, r{{[0-9]}}, r{{[0-9]}}
; CHECK-NEXT:  mov pc, ra
  %r = add i64 %a, %b
  ret i64 %r
}

define i64 @sub64(i64 %a, i64 %b) {
; CHECK-LABEL: sub64:
; CHECK:       subs r0, r0, r2
; CHECK-NEXT:  sbc r1, r1, r3
; CHECK-NEXT:  mov pc, ra
  %r = sub i64 %a, %b
  ret i64 %r
}

define i64 @neg64(i64 %a) {
; CHECK-LABEL: neg64:
; CHECK:       subs r0, r{{[0-9]}}, r0
; CHECK-NEXT:  sbc r1, r{{[0-9]}}, r1
  %r = sub i64 0, %a
  ret i64 %r
}

; Immediate forms cover i64 +/- small constants.
define i64 @inc64(i64 %a) {
; CHECK-LABEL: inc64:
; CHECK:       add_imms r0, r0, 1
; CHECK-NEXT:  adc_imm r1, r1, 0
; CHECK-NEXT:  mov pc, ra
  %r = add i64 %a, 1
  ret i64 %r
}
