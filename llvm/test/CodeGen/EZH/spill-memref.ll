; RUN: llc -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O2 -stop-after=prologepilog < %s | FileCheck %s
;
; storeRegToStackSlot/loadRegFromStackSlot must attach a MachineMemOperand to
; the spill str / reload ldr so machine AA can disambiguate stack slots.

declare i32 @ext(i32)

define i32 @pressure(i32 %a, i32 %b, i32 %c, i32 %d, i32 %e, i32 %f) {
; CHECK-LABEL: name: pressure
; CHECK-DAG: STR {{.*}} :: (store (s32) into %stack.
; CHECK-DAG: LDR {{.*}} :: (load (s32) from %stack.
  %x = call i32 @ext(i32 %a)
  %s1 = add i32 %x, %b
  %s2 = add i32 %s1, %c
  %s3 = add i32 %s2, %d
  %s4 = add i32 %s3, %e
  %s5 = add i32 %s4, %f
  %y = call i32 @ext(i32 %s5)
  %z = add i32 %y, %s4
  ret i32 %z
}
