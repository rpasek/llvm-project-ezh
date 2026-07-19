; RUN: llc -verify-machineinstrs -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O3 < %s | FileCheck %s

; Low-ones masks and near-INT_MAX values materialize with the inverted
; shifted-immediate form (load_simmn = ~(imm11 << shift)) instead of a
; constant-pool slot plus a pc-relative load.

define i32 @mask16(i32 %x) {
; CHECK-LABEL: mask16:
; CHECK:       load_simmn r{{[0-9]}}, -65536
; CHECK-NOT:   ldr r{{[0-9]}}, pc
  %r = and i32 %x, 65535
  ret i32 %r
}

define i32 @intmax() {
; CHECK-LABEL: intmax:
; CHECK:       load_simmn r0, -2147483648
; CHECK-NEXT:  mov pc, ra
  ret i32 2147483647
}

; A global accessed at several offsets loads its base address once; the
; offsets fold into the load/store immediates instead of minting one pool
; entry per (global, offset).
%struct.S = type { i32, i32, i32 }
@s = external global %struct.S

define void @fields() {
; CHECK-LABEL: fields:
; CHECK:       ldr r[[B:[0-9]]], pc,
; CHECK-NOT:   ldr r{{[0-9]}}, pc,
; CHECK:       ldr r{{[0-9]}}, r[[B]], 0
; CHECK:       str r[[B]], r{{[0-9]}}, 4
entry:
  %x = load i32, ptr @s
  %py = getelementptr inbounds %struct.S, ptr @s, i32 0, i32 1
  store i32 %x, ptr %py
  ret void
}
