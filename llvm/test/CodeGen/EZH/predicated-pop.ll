; RUN: llc -verify-machineinstrs -mtriple=ezh-none-elf -O3 < %s | FileCheck %s

; When the if-converter predicates a return block whose return was folded
; into a pop-into-PC LDR_POST, the popd alias must keep the condition
; suffix: a bare "popd pc" in the text would re-assemble as an
; unconditional return and change semantics. (Object emission always
; encoded the predicate correctly; this pins the printer.)
define i32 @predicated_pop(i32 %x) {
; CHECK-LABEL: predicated_pop:
; CHECK:       sub_imms r1, r0, 0
; CHECK:       load_imm_ze r0, 1
; CHECK-NEXT:  popd_ze pc
; CHECK:       popd pc
entry:
  %c = icmp eq i32 %x, 0
  br i1 %c, label %a, label %b
a:
  ret i32 1
b:
  %m = mul i32 %x, 3
  ret i32 %m
}
