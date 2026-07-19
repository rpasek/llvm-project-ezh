; RUN: llc -verify-machineinstrs -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O2 < %s | FileCheck %s
; RUN: llc -verify-machineinstrs -mtriple=ezh-none-elf -O2 < %s | FileCheck %s --check-prefix=BS
; RUN: llc -verify-machineinstrs -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O2 -ezh-tight-loops=false < %s | FileCheck %s --check-prefix=OFF
; Debug info must not change the conversion decisions (meta instructions are
; skipped in every scan).
; RUN: opt -passes=debugify -S < %s | llc -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O2 | FileCheck %s --check-prefix=DBG

; EZHTightLoopFormation converts the canonical counted single-block loop into
; the tight_loop zero-overhead hardware loop (AN14650 E_TIGHT_LOOP,
; silicon-validated): Rend = a register holding the address AFTER the last
; repeated instruction (a constant-pool label of the exit block, latched by
; hardware at loop entry), Rcount = n-1 (the block runs count+1 times), and
; the instruction right after tight_loop executes exactly once -- the pass
; parks a nop there so the repeated block is exactly the original body.
;
; In bitslice-interrupt mode the conversion is disabled: the interrupt
; workaround polls at branches, and a hardware loop has no per-iteration
; branch to poll at.

; The store pump: 3 instructions per iteration become 1.
define void @filln(ptr %p, i32 %x, i32 %n) {
; CHECK-LABEL: filln:
; CHECK:       goto_zb [[EXIT:.LBB[0-9_]+]]
; CHECK:       ldr [[END:r[0-9]]], pc, [[CPI:.LCPI[0-9_]+]]
; CHECK-NEXT:  add_imm [[CNT:r[0-9]]], [[CNT]], -1
; CHECK-NEXT:  tight_loop [[END]], [[CNT]]
; CHECK-NEXT:  nop
; CHECK-NEXT:  str_post
; CHECK-NEXT: [[EXIT]]:
; CHECK:      [[CPI]]:
; CHECK-NEXT:  .long [[EXIT]]
; CHECK-NOT:   goto_nz
;
; BS-LABEL: filln:
; BS-NOT:    tight_loop
; BS:        goto_nz
;
; OFF-LABEL: filln:
; OFF-NOT:   tight_loop
;
; DBG-LABEL: filln:
; DBG:       tight_loop
entry:
  %cmp = icmp sgt i32 %n, 0
  br i1 %cmp, label %loop, label %exit
loop:
  %i = phi i32 [ %inc, %loop ], [ 0, %entry ]
  %q = phi ptr [ %qn, %loop ], [ %p, %entry ]
  %qn = getelementptr inbounds nuw i8, ptr %q, i32 4
  store volatile i32 %x, ptr %q, align 4
  %inc = add nuw nsw i32 %i, 1
  %done = icmp eq i32 %inc, %n
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

; A multi-instruction body (byte copy) repeats as a unit.
define void @copy(ptr %d, ptr %s, i32 %n) {
; DBG-LABEL: copy:
; DBG:       tight_loop
; CHECK-LABEL: copy:
; CHECK:       tight_loop
; CHECK-NEXT:  nop
; CHECK-NEXT:  ldrb_post
; CHECK-NEXT:  strb_post
; CHECK-NOT:   goto_nz
entry:
  %cmp = icmp sgt i32 %n, 0
  br i1 %cmp, label %loop, label %exit
loop:
  %i = phi i32 [ %inc, %loop ], [ 0, %entry ]
  %dp = phi ptr [ %dn, %loop ], [ %d, %entry ]
  %sp = phi ptr [ %sn, %loop ], [ %s, %entry ]
  %sn = getelementptr inbounds nuw i8, ptr %sp, i32 1
  %v = load i8, ptr %sp, align 1
  %dn = getelementptr inbounds nuw i8, ptr %dp, i32 1
  store volatile i8 %v, ptr %dp, align 1
  %inc = add nuw nsw i32 %i, 1
  %done = icmp eq i32 %inc, %n
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

; The body stores the induction value, but LSR splits the counters: the body
; keeps its own up-counter while a separate down-counter drives the latch --
; still convertible, and the up-counter increment stays in the repeated block.
define void @counter_split(ptr %p, i32 %n) {
; DBG-LABEL: counter_split:
; DBG:       tight_loop
; CHECK-LABEL: counter_split:
; CHECK:       tight_loop
; CHECK-NEXT:  nop
; CHECK-NEXT:  str_post
; CHECK-NEXT:  add_imm
; CHECK-NOT:   goto_nz
entry:
  %cmp = icmp sgt i32 %n, 0
  br i1 %cmp, label %loop, label %exit
loop:
  %i = phi i32 [ %inc, %loop ], [ 0, %entry ]
  %q = phi ptr [ %qn, %loop ], [ %p, %entry ]
  %qn = getelementptr inbounds nuw i8, ptr %q, i32 4
  store volatile i32 %i, ptr %q, align 4
  %inc = add nuw nsw i32 %i, 1
  %done = icmp eq i32 %inc, %n
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

; The SAME register is the latch counter and a body operand (the stored value
; counts down): deleting the decrement would change what the body observes --
; must not fire.
define void @countdown_shared(ptr %p, i32 %n) {
; DBG-LABEL: countdown_shared:
; DBG-NOT:   tight_loop
; DBG:       goto_nz
; CHECK-LABEL: countdown_shared:
; CHECK-NOT:   tight_loop
; CHECK:       goto_nz
entry:
  %z = icmp eq i32 %n, 0
  br i1 %z, label %exit, label %loop
loop:
  %i = phi i32 [ %dec, %loop ], [ %n, %entry ]
  %q = phi ptr [ %qn, %loop ], [ %p, %entry ]
  %qn = getelementptr inbounds nuw i8, ptr %q, i32 4
  store volatile i32 %i, ptr %q, align 4
  %dec = add i32 %i, -1
  %done = icmp eq i32 %dec, 0
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

; A call in the body must not be hardware-looped.
declare void @f(i32)
define void @call_in_body(i32 %n) {
; CHECK-LABEL: call_in_body:
; CHECK-NOT:   tight_loop
; CHECK:       gosub f
entry:
  %cmp = icmp sgt i32 %n, 0
  br i1 %cmp, label %loop, label %exit
loop:
  %i = phi i32 [ %inc, %loop ], [ 0, %entry ]
  call void @f(i32 7)
  %inc = add nuw nsw i32 %i, 1
  %done = icmp eq i32 %inc, %n
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

; Inline asm in the body must not be hardware-looped: its emitted size is
; unknown (an empty asm would leave a ZERO-length repeated region and destroy
; the classic volatile-asm delay-loop idiom), and its text may hide branches.
define void @asm_delay(i32 %n) {
; CHECK-LABEL: asm_delay:
; CHECK-NOT:   tight_loop
; CHECK:       goto_nz
entry:
  %cmp = icmp sgt i32 %n, 0
  br i1 %cmp, label %loop, label %exit
loop:
  %i = phi i32 [ %inc, %loop ], [ 0, %entry ]
  call void asm sideeffect "", ""()
  %inc = add nuw nsw i32 %i, 1
  %done = icmp eq i32 %inc, %n
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

; optsize keeps the smaller compare-and-branch form.
define void @size_matters(ptr %p, i32 %x, i32 %n) optsize {
; CHECK-LABEL: size_matters:
; CHECK-NOT:   tight_loop
entry:
  %cmp = icmp sgt i32 %n, 0
  br i1 %cmp, label %loop, label %exit
loop:
  %i = phi i32 [ %inc, %loop ], [ 0, %entry ]
  %q = phi ptr [ %qn, %loop ], [ %p, %entry ]
  %qn = getelementptr inbounds nuw i8, ptr %q, i32 4
  store volatile i32 %x, ptr %q, align 4
  %inc = add nuw nsw i32 %i, 1
  %done = icmp eq i32 %inc, %n
  br i1 %done, label %exit, label %loop
exit:
  ret void
}
