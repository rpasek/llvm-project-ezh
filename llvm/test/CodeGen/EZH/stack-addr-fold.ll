; RUN: llc -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O3 < %s | FileCheck %s

; Stack-object accesses fold the FrameIndex (plus constant offset) straight
; into the memory operand: no separate add_imm to materialize the address.
; The only add_imm allowed here is the one that passes &buf to the callee.

declare void @use(ptr)

define i32 @word_locals() {
; CHECK-LABEL: word_locals:
; CHECK:       gosub use
; CHECK-NEXT:  ldr r0, sp, 4
; CHECK-NEXT:  ldr r1, sp, 8
; CHECK-NEXT:  add r0, r0, r1
; CHECK-NEXT:  str sp, r0, 0
entry:
  %buf = alloca [4 x i32], align 4
  call void @use(ptr %buf)
  %p1 = getelementptr inbounds [4 x i32], ptr %buf, i32 0, i32 1
  %v1 = load i32, ptr %p1
  %p2 = getelementptr inbounds [4 x i32], ptr %buf, i32 0, i32 2
  %v2 = load i32, ptr %p2
  %s = add i32 %v1, %v2
  store i32 %s, ptr %buf
  ret i32 %s
}

; Byte forms fold too (signed byte load shown).
define i32 @byte_locals() {
; CHECK-LABEL: byte_locals:
; CHECK:       gosub use
; CHECK-NEXT:  ldrbs r0, sp, 5
entry:
  %b = alloca [8 x i8], align 1
  call void @use(ptr %b)
  %p5 = getelementptr inbounds [8 x i8], ptr %b, i32 0, i32 5
  %v = load i8, ptr %p5
  %z = sext i8 %v to i32
  ret i32 %z
}
