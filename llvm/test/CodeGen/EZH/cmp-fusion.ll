; RUN: llc -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O3 < %s | FileCheck %s
; Debug info must not change the generated code: the pass skips DBG_VALUEs
; in its walk-back window and its dead-register scan.
; RUN: opt -passes=debugify -S < %s | llc -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O3 | FileCheck %s

; The EZHCompareFusion pass folds a compare-with-zero (sub_imms rD, rS, 0
; with rD dead) into the flag-setting twin of the adjacent instruction that
; produced rS, as long as every flag consumer to the end of the block uses a
; condition derived purely from the result value (ZE/NZ/PO/NE).

; The canonical loop latch: decrement + test + branch collapses to a
; flag-setting decrement + branch.
define void @loop_dec(ptr %d, ptr %s, i32 %n) {
; CHECK-LABEL: loop_dec:
; CHECK:       ldrb_post
; CHECK:       strb_post
; CHECK:       add_imms r{{[0-9]}}, r{{[0-9]}}, -1
; CHECK-NOT:   sub_imms
; CHECK:       goto_nz
entry:
  %cmp = icmp sgt i32 %n, 0
  br i1 %cmp, label %loop, label %exit
loop:
  %i = phi i32 [ %n, %entry ], [ %dec, %loop ]
  %pd = phi ptr [ %d, %entry ], [ %pd2, %loop ]
  %ps = phi ptr [ %s, %entry ], [ %ps2, %loop ]
  %v = load i8, ptr %ps
  store i8 %v, ptr %pd
  %ps2 = getelementptr i8, ptr %ps, i32 1
  %pd2 = getelementptr i8, ptr %pd, i32 1
  %dec = add nsw i32 %i, -1
  %c = icmp ne i32 %dec, 0
  br i1 %c, label %loop, label %exit
exit:
  ret void
}

; Logical producers fuse too: (a & b) == 0 becomes ands + a ZE-predicated
; consumer, no separate compare.
define i32 @and_test(i32 %a, i32 %b, i32 %x, i32 %y) {
; CHECK-LABEL: and_test:
; CHECK:       ands r0, r0, r1
; CHECK-NOT:   sub_imms
; CHECK:       mov_ze
  %m = and i32 %a, %b
  %c = icmp eq i32 %m, 0
  %r = select i1 %c, i32 %x, i32 %y
  ret i32 %r
}

; The producer does not need to be adjacent: loop bookkeeping (the counter
; increment) sits between the and and its test, and the pass walks back over
; it because it is flag-transparent.
define i32 @popcount(i32 %a) {
; CHECK-LABEL: popcount:
; CHECK:       ands r{{[0-9]}}, r{{[0-9]}}, r{{[0-9]}}
; CHECK-NOT:   sub_imms
; CHECK:       goto_nz
entry:
  %tobool = icmp eq i32 %a, 0
  br i1 %tobool, label %exit, label %loop
loop:
  %x = phi i32 [ %and, %loop ], [ %a, %entry ]
  %n = phi i32 [ %inc, %loop ], [ 0, %entry ]
  %dec = add i32 %x, -1
  %and = and i32 %dec, %x
  %inc = add i32 %n, 1
  %again = icmp ne i32 %and, 0
  br i1 %again, label %loop, label %exit
exit:
  %r = phi i32 [ 0, %entry ], [ %inc, %loop ]
  ret i32 %r
}

; Negative case: the tested value arrives in a register with no producer
; instruction in the block (an incoming argument), so the compare must stay.
define i32 @no_producer(i32 %c, i32 %a, i32 %b) {
; CHECK-LABEL: no_producer:
; CHECK:       sub_imms r0, r0, 0
; CHECK:       mov_ze
  %t = icmp eq i32 %c, 0
  %r = select i1 %t, i32 %a, i32 %b
  ret i32 %r
}

; Negative case: optnone functions inside an optimized TU keep their
; compares (the pass calls skipFunction).
define i32 @optnone_keeps_cmp(i32 %a, i32 %x, i32 %y) noinline optnone {
; CHECK-LABEL: optnone_keeps_cmp:
; CHECK:       sub_imms
  %m = and i32 %a, 7
  %c = icmp eq i32 %m, 0
  %r = select i1 %c, i32 %x, i32 %y
  ret i32 %r
}
