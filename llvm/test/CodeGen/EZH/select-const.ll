; RUN: llc -verify-machineinstrs -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O3 < %s | FileCheck %s

; A select of an immediate folds the materialization into the predicated
; slot: the if-converter produces load_imm_cc dst, K instead of
; materialize-then-mov_cc, and the standalone load disappears.

define i32 @pick(i32 %c, i32 %x) {
; CHECK-LABEL: pick:
; CHECK:       load_imm r0, 7
; CHECK-NEXT:  load_imm_ze r0, 42
; CHECK-NEXT:  mov pc, ra
; CHECK-NOT:   mov_
  %t = icmp eq i32 %c, 0
  %r = select i1 %t, i32 42, i32 7
  ret i32 %r
}

; Only the false side is a constant: the sides swap and the condition
; inverts, so the fold still lands.
define i32 @false_const(i32 %c, i32 %x) {
; CHECK-LABEL: false_const:
; CHECK:       load_imm_nz r{{[0-9]}}, 9
  %t = icmp eq i32 %c, 0
  %r = select i1 %t, i32 %x, i32 9
  ret i32 %r
}

; Inverted-immediate materializations (load_simmn) fold the same way.
define i32 @mask_or_val(i32 %c, i32 %y) {
; CHECK-LABEL: mask_or_val:
; CHECK:       load_simmn_nz r{{[0-9]}}, -65536
  %t = icmp eq i32 %c, 0
  %r = select i1 %t, i32 %y, i32 65535
  ret i32 %r
}
