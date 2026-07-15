; RUN: llc -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O3 < %s | FileCheck %s
; RUN: llc -mtriple=ezh-none-elf -O3 < %s | FileCheck %s --check-prefix=BS

declare i32 @g(i32)
declare void @h()
declare i32 @many(i32, i32, i32, i32, i32)

; A sibling call is a plain goto: no RA save, no gosub, no return.
; With bitslice interrupts (BS) the tail call keeps its interrupt poll --
; a cycle of unconditional tail calls must not starve the handler -- but
; the poll sits BEFORE the epilogue, where RA's live value is still in its
; stack slot; after popd ra nothing may clobber RA until the callee saves
; it again.
define i32 @sibcall(i32 %x) {
; CHECK-LABEL: sibcall:
; CHECK-NOT:   pushd
; CHECK:       add_imm r0, r0, 1
; CHECK-NEXT:  goto g
; CHECK-NOT:   gosub
; CHECK-NOT:   popd

; BS-LABEL: sibcall:
; BS:       pushd ra
; BS:       add_imm r0, r0, 1
; BS-NEXT:  gotol_bs bitslice_handler
; BS-NEXT:  popd ra
; BS-NEXT:  goto g
  %y = add i32 %x, 1
  %r = tail call i32 @g(i32 %y)
  ret i32 %r
}

define void @wrapper() {
; CHECK-LABEL: wrapper:
; CHECK:       goto h
; CHECK-NOT:   gosub
  tail call void @h()
  ret void
}

; Negative: the fifth argument travels on the stack, which cannot survive a
; sibling call; this must stay a real call.
define i32 @stack_args(i32 %a, i32 %b, i32 %c, i32 %d, i32 %e) {
; CHECK-LABEL: stack_args:
; CHECK:       gosub many
; CHECK:       popd pc
  %r = tail call i32 @many(i32 %a, i32 %b, i32 %c, i32 %d, i32 %e)
  ret i32 %r
}

; Negative: only direct symbols are goto-reachable; an indirect tail call
; stays a real call.
define i32 @indirect(ptr %fp, i32 %x) {
; CHECK-LABEL: indirect:
; CHECK:       goto_regl
; CHECK:       popd pc
  %r = tail call i32 %fp(i32 %x)
  ret i32 %r
}

; With bitslice interrupts, a function that mixes a real call with a tail
; call keeps the RA save and restores RA (not PC) before the final goto.
define i32 @mixed(i32 %x) {
; BS-LABEL: mixed:
; BS:       pushd ra
; BS:       gosub h
; BS:       popd ra
; BS-NEXT:  goto g
  tail call void @h()
  %r = tail call i32 @g(i32 %x)
  ret i32 %r
}
