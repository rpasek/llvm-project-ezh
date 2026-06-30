; RUN: llc -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O2 < %s | FileCheck %s
;
; Register-offset load/store (opcode 0x1D, address = Rn + Rm as a byte offset):
; a runtime-indexed access folds the address add into one instruction instead of
; a separate add + immediate-offset access. Hardware-validated on an
; EVK-MIMXRT595, including the sign-extending byte load -- ldr_regbs uses
; Inst{29} for the sign-extend control, not the Inst{21} the immediate-offset
; form uses (0x1D ignores bit 21).

define i32 @word_load(ptr %a, i32 %i) {
; CHECK-LABEL: word_load:
; CHECK: ldr_reg r0, r0, r1
  %p = getelementptr i32, ptr %a, i32 %i
  %v = load i32, ptr %p
  ret i32 %v
}

define void @word_store(ptr %a, i32 %i, i32 %v) {
; CHECK-LABEL: word_store:
; CHECK: str_reg
  %p = getelementptr i32, ptr %a, i32 %i
  store i32 %v, ptr %p
  ret void
}

define i32 @ubyte_load(ptr %a, i32 %i) {
; CHECK-LABEL: ubyte_load:
; CHECK: ldr_regb r0, r0, r1
  %p = getelementptr i8, ptr %a, i32 %i
  %v = load i8, ptr %p
  %z = zext i8 %v to i32
  ret i32 %z
}

define i32 @sbyte_load(ptr %a, i32 %i) {
; CHECK-LABEL: sbyte_load:
; CHECK: ldr_regbs r0, r0, r1
  %p = getelementptr i8, ptr %a, i32 %i
  %v = load i8, ptr %p
  %s = sext i8 %v to i32
  ret i32 %s
}

define void @byte_store(ptr %a, i32 %i, i32 %v) {
; CHECK-LABEL: byte_store:
; CHECK: str_regb
  %p = getelementptr i8, ptr %a, i32 %i
  %t = trunc i32 %v to i8
  store i8 %t, ptr %p
  ret void
}
