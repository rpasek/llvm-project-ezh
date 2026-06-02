; RUN: llc -mtriple=ezh-none-elf < %s | FileCheck %s

define i32 @test_jumptable(i32 %x) {
; CHECK-LABEL: test_jumptable:
; CHECK:       ; %bb.0:
; CHECK-NEXT:    pushd ra
; CHECK-NEXT:    sub_imm sp, sp, 4
; CHECK-NEXT:    load_imm r1, 3
; CHECK-NEXT:    subs r1, r1, r0
; CHECK-NEXT:    gotol_bs bitslice_handler
; CHECK-NEXT:    goto_ca .LBB0_7
; CHECK-NEXT:  ; %bb.1:
; CHECK-NEXT:  [[LABEL:\.Ltmp[0-9]+]]:
; CHECK-NEXT:    add_imm r1, pc, .LJTI0_0-([[LABEL]]+8)
; CHECK-NEXT:    lsl_add r0, r1, r0, 2
; CHECK-NEXT:    ldr r0, r0, 0
; CHECK-NEXT:    gotol_bs bitslice_handler
; CHECK-NEXT:    mov pc, r0
; CHECK-NEXT:  ; %bb.2:
; CHECK-NEXT:    .p2align 2
; CHECK-NEXT:  .LJTI0_0:
; CHECK-NEXT:    .long .LBB0_3
; CHECK-NEXT:    .long .LBB0_6
; CHECK-NEXT:    .long .LBB0_4
; CHECK-NEXT:    .long .LBB0_5
; CHECK:       .LBB0_3:
; CHECK:         load_imm r0, 10
; CHECK-NEXT:    add_imm sp, sp, 4
; CHECK-NEXT:    popd pc
; CHECK:       .LBB0_4:
; CHECK:         load_imm r0, 30
; CHECK-NEXT:    add_imm sp, sp, 4
; CHECK-NEXT:    popd pc
; CHECK:       .LBB0_5:
; CHECK:         load_imm r0, 40
; CHECK-NEXT:    add_imm sp, sp, 4
; CHECK-NEXT:    popd pc
; CHECK:       .LBB0_6:
; CHECK:         load_imm r0, 20
; CHECK-NEXT:    add_imm sp, sp, 4
; CHECK-NEXT:    popd pc
; CHECK:       .LBB0_7:
; CHECK:         load_imm r0, 0
; CHECK-NEXT:    add_imm sp, sp, 4
; CHECK-NEXT:    popd pc

entry:
  switch i32 %x, label %default [
    i32 0, label %sw.bb0
    i32 1, label %sw.bb1
    i32 2, label %sw.bb2
    i32 3, label %sw.bb3
  ]

sw.bb0:
  ret i32 10
sw.bb1:
  ret i32 20
sw.bb2:
  ret i32 30
sw.bb3:
  ret i32 40
default:
  ret i32 0
}
