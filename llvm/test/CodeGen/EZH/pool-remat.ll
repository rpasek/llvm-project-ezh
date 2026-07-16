; RUN: llc -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O3 < %s | FileCheck %s

; LOAD_CONSTANT carries an invariant constant-pool MMO and is marked
; rematerializable, so a pool constant that lives across a call is
; re-loaded from the pool instead of being parked in a callee-saved
; register or spilled to the stack.
declare void @use(i32, i32, i32, i32)

define i32 @presspill(i32 %a, i32 %b, i32 %c, i32 %d) {
; CHECK-LABEL: presspill:
; CHECK:       gosub use
; CHECK:       ldr r{{[0-9]}}, pc,
; CHECK:       gosub use
; CHECK:       ldr r{{[0-9]}}, pc,
; CHECK-NOT:   str sp
entry:
  call void @use(i32 %a, i32 %b, i32 %c, i32 %d)
  %v = add i32 %a, 1234567
  call void @use(i32 %v, i32 %b, i32 %c, i32 %d)
  %w = add i32 %v, 1234567
  ret i32 %w
}

; The immediate-materialization families are rematerializable too (their
; unpredicated encodings), so a constant needed after a call is re-created
; there rather than surviving in a register across it. Here it folds all
; the way into the add itself.
define i32 @imm_remat(i32 %a, i32 %b, i32 %c, i32 %d) {
; CHECK-LABEL: imm_remat:
; CHECK:       gosub use
; CHECK:       add_imm r0, r{{[0-9]}}, 777
entry:
  call void @use(i32 %a, i32 %b, i32 %c, i32 %d)
  %v = add i32 %a, 777
  ret i32 %v
}

; The load itself must rematerialize: a constant live before and after the
; call is re-created by a second load_imm, not parked in a callee-saved
; register (external review caught the flag-setting format class
; overriding the remat eligibility here).
define i32 @imm_live_across(i32 %a, i32 %b, i32 %c, i32 %d) {
; CHECK-LABEL: imm_live_across:
; CHECK-NOT:   pushd r4
; CHECK:       load_imm r0, 777
; CHECK:       gosub use
; CHECK-NEXT:  load_imm r0, 777
; CHECK-NEXT:  popd pc
entry:
  call void @use(i32 777, i32 %b, i32 %c, i32 %d)
  ret i32 777
}

; Shifted-immediate form, same requirement.
define i32 @simm_live_across(i32 %a, i32 %b, i32 %c, i32 %d) {
; CHECK-LABEL: simm_live_across:
; CHECK-NOT:   pushd r4
; CHECK:       load_simm r0, 81920
; CHECK:       gosub use
; CHECK-NEXT:  load_simm r0, 81920
entry:
  call void @use(i32 81920, i32 %b, i32 %c, i32 %d)
  ret i32 81920
}
