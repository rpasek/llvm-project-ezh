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

; An indirect tail call becomes goto_reg (no RA write); the address lives
; in a register that survives the epilogue (GPRTC = r0-r3).
define i32 @indirect(ptr %fp, i32 %x) {
; CHECK-LABEL: indirect:
; CHECK-NOT:   pushd
; CHECK:       goto_reg r{{[0-3]}}
; CHECK-NOT:   goto_regl
; CHECK-NOT:   popd
  %r = tail call i32 %fp(i32 %x)
  ret i32 %r
}

declare i32 @g5(i32, i32, i32, i32, i32)
declare i32 @g6(i32, i32, i32, i32, i32, i32)
declare i32 @vf(i32, ...)

; musttail with a stack argument: matching prototypes make the caller's
; incoming slot the callee's incoming slot, so the fifth argument is
; rewritten in place (SP-relative, no separate address materialization)
; and the goto happens with SP back at the entry value.
define i32 @musttail_stack(i32 %a, i32 %b, i32 %c, i32 %d, i32 %e) {
; CHECK-LABEL: musttail_stack:
; CHECK:       ldr r{{[0-9]}}, sp, {{[0-9]+}}
; CHECK:       str sp, r{{[0-9]}}, {{[0-9]+}}
; CHECK:       goto g5
; CHECK-NOT:   gosub
  %e2 = add i32 %e, 1
  %r = musttail call i32 @g5(i32 %a, i32 %b, i32 %c, i32 %d, i32 %e2)
  ret i32 %r
}

; Swapping two stack arguments: both old values must be loaded before
; either slot is overwritten (getStackArgumentTokenFactor ordering).
define i32 @musttail_swap(i32 %a, i32 %b, i32 %c, i32 %d, i32 %e, i32 %f) {
; CHECK-LABEL: musttail_swap:
; CHECK:       ldr
; CHECK:       ldr
; CHECK:       str
; CHECK:       str
; CHECK:       goto g6
  %r = musttail call i32 @g6(i32 %a, i32 %b, i32 %c, i32 %d, i32 %f, i32 %e)
  ret i32 %r
}

; musttail in a vararg pair forwards the ellipsis: the unnamed argument
; registers are re-presented to the callee (here they coalesce to no code)
; and the register save area is deallocated before the goto.
define i32 @musttail_vararg(i32 %x, ...) {
; CHECK-LABEL: musttail_vararg:
; CHECK:       add_imm r0, r0, 7
; CHECK:       add_imm sp, sp, 12
; CHECK-NEXT:  goto vf
  %x2 = add i32 %x, 7
  %r = musttail call i32 (i32, ...) @vf(i32 %x2, ...)
  ret i32 %r
}

@fptr = external global ptr

; With all four argument registers occupied, no epilogue-surviving register
; can carry an indirect target, so an ordinary tail call falls back to a
; real call rather than failing register allocation.
define i32 @indirect_4args_fallback(i32 %a, i32 %b, i32 %c, i32 %d) {
; CHECK-LABEL: indirect_4args_fallback:
; CHECK:       goto_regl
; CHECK:       popd pc
  %fp = load ptr, ptr @fptr
  %r = tail call i32 %fp(i32 %a, i32 %b, i32 %c, i32 %d)
  ret i32 %r
}

; The same shape as musttail must still be honored: the target is parked in
; a stack slot before the epilogue and loaded straight into PC after the
; frame teardown (a small negative offset from the restored entry SP), so
; it needs no register at all.
define i32 @musttail_indirect_4args(i32 %a, i32 %b, i32 %c, i32 %d) {
; CHECK-LABEL: musttail_indirect_4args:
; CHECK:       str
; CHECK-NOT:   goto_reg
; CHECK:       ldr pc, sp, -{{[0-9]+}}
  %fp = load ptr, ptr @fptr
  %r = musttail call i32 %fp(i32 %a, i32 %b, i32 %c, i32 %d)
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
