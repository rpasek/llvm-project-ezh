; RUN: llc -verify-machineinstrs -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O3 < %s | FileCheck %s

; GlobalMerge runs in the EZH pipeline: small globals merge into one blob
; whose single pooled base address covers every member through folded
; load/store offsets, instead of one pool entry and one pc-relative load
; per global per function.

@a = dso_local local_unnamed_addr global i32 0, align 4
@b = dso_local local_unnamed_addr global i32 0, align 4
@c = dso_local local_unnamed_addr global i32 0, align 4

define void @touch() {
; CHECK-LABEL: touch:
; CHECK:       ldr r[[B:[0-9]]], pc,
; CHECK-NOT:   ldr r{{[0-9]}}, pc,
; CHECK-DAG:   ldr r{{[0-9]}}, r[[B]],
; CHECK-DAG:   str r[[B]], r{{[0-9]}},
entry:
  %vb = load i32, ptr @b
  %vc = load i32, ptr @c
  %s = add i32 %vb, %vc
  store i32 %s, ptr @a
  ret void
}

; CHECK: .L_MergedGlobals
