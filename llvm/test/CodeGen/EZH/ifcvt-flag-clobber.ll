; RUN: llc -verify-machineinstrs -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O2 < %s | FileCheck %s

; A block containing a flag-writing S-form must NOT be if-converted. The i64
; add lowers to adds/adc; if the if-converter predicated the arm, the executed
; adds_ze would overwrite the condition flags and the following adc_ze would
; test the ADDITION's flags instead of the compare's -- silent wrong code
; (e.g. hi-word never computed when the lo-word sum is nonzero). With the
; flags modelled as CFS, ClobbersPredicate reports the adds and the
; if-converter keeps the arm behind a real branch.

define i64 @no_ifcvt_sform_arm(i32 %c, i64 %a, i64 %b) {
; CHECK-LABEL: no_ifcvt_sform_arm:
; CHECK-NOT: adds_
; CHECK-NOT: adc_
; CHECK: adds r{{[0-9]}}
; CHECK-NEXT: adc r{{[0-9]}}
entry:
  %t = icmp eq i32 %c, 0
  br i1 %t, label %arm, label %join
arm:
  %s = add i64 %a, %b
  br label %join
join:
  %r = phi i64 [ %s, %arm ], [ %a, %entry ]
  ret i64 %r
}

; Arms without a flag writer still if-convert (the predicated ALU forms).
define i32 @still_ifcvt_pure_arm(i32 %c, i32 %x, i32 %y) {
; CHECK-LABEL: still_ifcvt_pure_arm:
; CHECK: add_ze r{{[0-9]}}
entry:
  %t = icmp eq i32 %c, 0
  br i1 %t, label %arm, label %join
arm:
  %s = add i32 %x, %y
  br label %join
join:
  %r = phi i32 [ %s, %arm ], [ %x, %entry ]
  ret i32 %r
}
