; RUN: llc -verify-machineinstrs -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O2 -stop-after=finalize-isel < %s | FileCheck %s
;
; The pre/post-indexed loads/stores are selected manually in EZHISelDAGToDAG
; (empty tablegen patterns), so they must (a) be marked mayLoad/mayStore and
; (b) carry the original access's MachineMemOperand -- otherwise machine AA and
; the post-RA scheduler treat them as non-memory. Check the MMO is attached.

define void @copy(ptr %d, ptr %s, i32 %n) {
; CHECK-LABEL: name: copy
; CHECK: LDR_POST {{.*}} :: (load (s32) from %ir.
; CHECK: STR_POST {{.*}} :: (store (s32) into %ir.
entry:
  %c = icmp sgt i32 %n, 0
  br i1 %c, label %loop, label %exit
loop:
  %i = phi i32 [ 0, %entry ], [ %i.n, %loop ]
  %sp = phi ptr [ %s, %entry ], [ %sp.n, %loop ]
  %dp = phi ptr [ %d, %entry ], [ %dp.n, %loop ]
  %v = load i32, ptr %sp, align 4
  store i32 %v, ptr %dp, align 4
  %sp.n = getelementptr inbounds i32, ptr %sp, i32 1
  %dp.n = getelementptr inbounds i32, ptr %dp, i32 1
  %i.n = add i32 %i, 1
  %e = icmp eq i32 %i.n, %n
  br i1 %e, label %exit, label %loop
exit:
  ret void
}
