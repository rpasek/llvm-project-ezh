; RUN: llc -verify-machineinstrs -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O3 < %s | FileCheck %s

; EZH multiplies through the __mulsi3 libcall, so decomposeMulByConstant
; accepts every constant shape the DAG combiner can turn into shifts and
; adds. The shifted-ALU forms then fold each shift+add/sub pair into one
; instruction.

define i32 @mul3(i32 %a) {
; CHECK-LABEL: mul3:
; CHECK:       lsl_add r0, r0, r0, 1
; CHECK-NOT:   __mulsi3
  %r = mul i32 %a, 3
  ret i32 %r
}

define i32 @mul7(i32 %a) {
; CHECK-LABEL: mul7:
; CHECK:       lsl_sub r0, r0, r0, 3
; CHECK-NOT:   __mulsi3
  %r = mul i32 %a, 7
  ret i32 %r
}

; 320 = (a << 8) + (a << 6)
define i32 @mul320(i32 %a) {
; CHECK-LABEL: mul320:
; CHECK:       lsl r1, r0, 8
; CHECK-NEXT:  lsl_add r0, r1, r0, 6
; CHECK-NOT:   __mulsi3
  %r = mul i32 %a, 320
  ret i32 %r
}

; Beyond the generic combiner's shapes, the target combine synthesizes
; short chains: 100 = (8*3 + 1) * 4, three shifted-ALU instructions.
define i32 @mul100(i32 %a) {
; CHECK-LABEL: mul100:
; CHECK:       lsl_add r1, r0, r0, 1
; CHECK-NEXT:  lsl_add r0, r0, r1, 3
; CHECK-NEXT:  lsl r0, r0, 2
; CHECK-NOT:   __mulsi3
  %r = mul i32 %a, 100
  ret i32 %r
}

; 365 = (8*9 + 1) * 5: the last factor-of-five step multiplies the
; accumulator in place, so no final shift is needed.
define i32 @mul365(i32 %a) {
; CHECK-LABEL: mul365:
; CHECK:       lsl_add r1, r0, r0, 3
; CHECK-NEXT:  lsl_add r0, r0, r1, 3
; CHECK-NEXT:  lsl_add r0, r0, r0, 2
; CHECK-NOT:   __mulsi3
  %r = mul i32 %a, 365
  ret i32 %r
}

; No chain of at most three instructions exists: stays a libcall, and as
; the sole use feeds the return it tail-calls the helper.
define i32 @mul1234567(i32 %a) {
; CHECK-LABEL: mul1234567:
; CHECK:       goto __mulsi3
; CHECK-NOT:   gosub
  %r = mul i32 %a, 1234567
  ret i32 %r
}
