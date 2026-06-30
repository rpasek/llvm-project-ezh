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
