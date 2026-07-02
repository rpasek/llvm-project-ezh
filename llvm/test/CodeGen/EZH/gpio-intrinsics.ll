; RUN: llc -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O2 < %s | FileCheck %s
;
; The GPIO intrinsics select to single special-register instructions. GPI/GPD
; are reserved registers, hardwired into these defs. Hardware-validated on an
; EVK-MIMXRT595 (GPD drive/read/release round-trips through bit 5).

declare i32 @llvm.ezh.read.gpi()
declare i32 @llvm.ezh.read.gpd()
declare void @llvm.ezh.gpd.drive.low(i32 immarg)
declare void @llvm.ezh.gpd.release(i32 immarg)

define i32 @read_gpi() {
; CHECK-LABEL: read_gpi:
; CHECK: mov r0, gpi
  %v = call i32 @llvm.ezh.read.gpi()
  ret i32 %v
}
define i32 @read_gpd() {
; CHECK-LABEL: read_gpd:
; CHECK: mov r0, gpd
  %v = call i32 @llvm.ezh.read.gpd()
  ret i32 %v
}
define void @drive() {
; CHECK-LABEL: drive:
; CHECK: bset_imm gpd, gpd, 7
  call void @llvm.ezh.gpd.drive.low(i32 7)
  ret void
}
define void @release() {
; CHECK-LABEL: release:
; CHECK: bclr_imm gpd, gpd, 3
  call void @llvm.ezh.gpd.release(i32 3)
  ret void
}

declare i32 @llvm.ezh.read.gpo()
declare void @llvm.ezh.write.gpo(i32)
declare void @llvm.ezh.write.gpd(i32)

define i32 @t_gpo_rw(i32 %v) {
; CHECK-LABEL: t_gpo_rw:
; CHECK: mov gpo, r0
; CHECK: mov r0, gpo
  call void @llvm.ezh.write.gpo(i32 %v)
  %r = call i32 @llvm.ezh.read.gpo()
  ret i32 %r
}
define void @t_gpd_w(i32 %m) {
; CHECK-LABEL: t_gpd_w:
; CHECK: mov gpd, r0
  call void @llvm.ezh.write.gpd(i32 %m)
  ret void
}
