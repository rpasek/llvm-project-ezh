; RUN: llc -verify-machineinstrs -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O2 < %s | FileCheck %s

; A sign-extending i1 load off a stack slot must produce 0/-1, i.e. a byte load
; followed by a 1-bit sign fixup (negate) -- never a bare zero-extending byte
; load, which would silently yield 0/1. The stack-address fold selects frame
; loads/stores directly (tryFrameLoadStore); it must not absorb the sextloadi1
; and drop the sign extension.

declare void @clobber()

; CHECK-LABEL: sext_i1:
; The i1 is reloaded as a zero-extended byte, then negated (0 - x) to sign-
; extend bit 0 into 0/-1.
; CHECK:      ldrb [[R:r[0-9]+]], sp, 0
; CHECK:      load_imm [[Z:r[0-9]+]], 0
; CHECK:      sub {{r[0-9]+}}, [[Z]], [[R]]
define i32 @sext_i1(i1 %c) {
  %p = alloca i1
  store i1 %c, ptr %p
  call void @clobber()
  %v = load i1, ptr %p
  %r = sext i1 %v to i32
  ret i32 %r
}

; A zero-extending i1 load is just the byte load, with no sign fixup.
; CHECK-LABEL: zext_i1:
; CHECK:      ldrb {{r[0-9]+}}, sp, 0
; CHECK-NOT:  sub {{.*}}, {{.*}}, {{.*}}
; CHECK:      popd pc
define i32 @zext_i1(i1 %c) {
  %p = alloca i1
  store i1 %c, ptr %p
  call void @clobber()
  %v = load i1, ptr %p
  %r = zext i1 %v to i32
  ret i32 %r
}
