; RUN: llc -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O2 < %s | FileCheck %s
;
; The EZH event/control-fabric intrinsics select to their instructions: hold,
; int_trigger, the CFM/CFS special-register movs, the beat/heart-rythm timing
; primitives, and the GPO read-modify-write. The special-register movs are
; isCodeGenOnly (the assembler/disassembler use the generic mov with the
; register name).

declare void @llvm.ezh.hold()
declare void @llvm.ezh.int.trigger(i32 immarg)
declare i32 @llvm.ezh.read.cfm()
declare void @llvm.ezh.write.cfm(i32)
declare i32 @llvm.ezh.read.cfs()
declare void @llvm.ezh.write.cfs(i32)
declare void @llvm.ezh.wait.for.beat()
declare void @llvm.ezh.synch.all.to.beat(i32 immarg)
declare void @llvm.ezh.heart.rythm(i32)
declare void @llvm.ezh.heart.rythm.imm(i32 immarg)
declare void @llvm.ezh.modify.gpo.byte(i32 immarg, i32 immarg, i32 immarg)

define void @t_hold() {
; CHECK-LABEL: t_hold:
; CHECK: hold
  call void @llvm.ezh.hold()
  ret void
}
define void @t_it() {
; CHECK-LABEL: t_it:
; CHECK: int_trigger 42
  call void @llvm.ezh.int.trigger(i32 42)
  ret void
}
define i32 @t_rcfm() {
; CHECK-LABEL: t_rcfm:
; CHECK: mov r0, cfm
  %v = call i32 @llvm.ezh.read.cfm()
  ret i32 %v
}
define void @t_wcfm(i32 %x) {
; CHECK-LABEL: t_wcfm:
; CHECK: mov cfm, r0
  call void @llvm.ezh.write.cfm(i32 %x)
  ret void
}
define i32 @t_rcfs() {
; CHECK-LABEL: t_rcfs:
; CHECK: mov r0, cfs
  %v = call i32 @llvm.ezh.read.cfs()
  ret i32 %v
}
define void @t_wcfs(i32 %x) {
; CHECK-LABEL: t_wcfs:
; CHECK: mov cfs, r0
  call void @llvm.ezh.write.cfs(i32 %x)
  ret void
}
define void @t_wfb() {
; CHECK-LABEL: t_wfb:
; CHECK: wait_for_beat
  call void @llvm.ezh.wait.for.beat()
  ret void
}
define void @t_satb() {
; CHECK-LABEL: t_satb:
; CHECK: synch_all_to_beat 1
  call void @llvm.ezh.synch.all.to.beat(i32 1)
  ret void
}
define void @t_hr(i32 %x) {
; CHECK-LABEL: t_hr:
; CHECK: heart_rythm r0
  call void @llvm.ezh.heart.rythm(i32 %x)
  ret void
}
define void @t_hri() {
; CHECK-LABEL: t_hri:
; CHECK: heart_rythm_imm 1000
  call void @llvm.ezh.heart.rythm.imm(i32 1000)
  ret void
}
define void @t_mod() {
; CHECK-LABEL: t_mod:
; CHECK: modify_gpo_byte 255, 0, 15
  call void @llvm.ezh.modify.gpo.byte(i32 255, i32 0, i32 15)
  ret void
}

declare void @llvm.ezh.tight.loop(ptr, i32)

; The tight_loop hardware loop: Rcount materialized as an immediate, Rend as a
; PC-relative literal-pool load of the after-body block address.
define void @t_tight_loop() {
; CHECK-LABEL: t_tight_loop:
; CHECK-DAG: load_imm [[CNT:r[0-9]+]], 6
; CHECK-DAG: ldr [[END:r[0-9]+]], pc, .LCPI[[L:[0-9_]+]]
; CHECK: tight_loop [[END]], [[CNT]]
; CHECK-NEXT: hold
; CHECK: .LCPI[[L]]:
; CHECK-NEXT: .long .Ltmp0
entry:
  call void @llvm.ezh.tight.loop(ptr blockaddress(@t_tight_loop, %end), i32 6)
  call void @llvm.ezh.hold()
  br label %end
end:
  ret void
}

declare void @llvm.ezh.cfm.bset(i32 immarg)
declare void @llvm.ezh.cfm.bclr(i32 immarg)
declare void @llvm.ezh.nop()

define void @t_cfm_bits() {
; CHECK-LABEL: t_cfm_bits:
; CHECK: bset_imm cfm, cfm, 3
; CHECK: bclr_imm cfm, cfm, 0
  call void @llvm.ezh.cfm.bset(i32 3)
  call void @llvm.ezh.cfm.bclr(i32 0)
  ret void
}
define void @t_nop() {
; CHECK-LABEL: t_nop:
; CHECK: nop
  call void @llvm.ezh.nop()
  ret void
}

declare ptr @llvm.ezh.acc.vectored.hold(ptr, i32 immarg)

define ptr @t_acc_vectored_hold(ptr %table) {
; CHECK-LABEL: t_acc_vectored_hold:
; The Rd operand must differ from the table register (r0): $Rd is
; @earlyclobber because the same-register form does not deliver a usable
; vector on silicon.
; CHECK: acc_vectored_hold r{{[1-7]}}, r0, 255
  %v = call ptr @llvm.ezh.acc.vectored.hold(ptr %table, i32 255)
  ret ptr %v
}
