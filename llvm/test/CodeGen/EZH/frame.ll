; RUN: llc -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -frame-pointer=all -O3 < %s | FileCheck %s --check-prefix=FP
; RUN: llc -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -frame-pointer=none -O3 < %s | FileCheck %s --check-prefix=NOFP

; Leaf function: no locals, no calls -> no frame at all without FP. The
; emergency scavenging slot is only created when the estimated stack size
; approaches the load/store offset range, so small frames stay empty.
define void @test_leaf() {
; FP-LABEL: test_leaf:
; FP:       pushd r7
; FP:       mov r7, sp
; FP:       mov sp, r7
; FP:       popd r7
; FP:       mov pc, ra

; NOFP-LABEL: test_leaf:
; NOFP-NOT:   sub_imm sp
; NOFP-NOT:   pushd
; NOFP:       mov pc, ra
entry:
  ret void
}

; Non-leaf function: RA save only, no local frame.
define void @test_non_leaf() {
; FP-LABEL: test_non_leaf:
; FP:       pushd ra
; FP:       pushd r7
; FP:       mov r7, sp
; FP:       gosub use_fp
; FP:       mov sp, r7
; FP:       popd r7
; FP:       popd pc

; NOFP-LABEL: test_non_leaf:
; NOFP:       pushd ra
; NOFP-NOT:   sub_imm sp
; NOFP:       gosub use_fp
; NOFP-NOT:   add_imm sp
; NOFP:       popd pc
entry:
  call void @use_fp()
  ret void
}

declare void @use_fp()

; Function with CSRs and FP
define void @test_csr() {
; FP-LABEL: test_csr:
; FP:       pushd ra
; FP:       pushd r7
; FP:       pushd r5
; FP:       pushd r4
; FP:       add_imm r7, sp, 8
; ...
; FP:       sub_imm sp, r7, 8
; FP:       popd r4
; FP:       popd r5
; FP:       popd r7
; FP:       popd pc
entry:
  ; Use many registers to force CSR spills
  %r4 = call i32 @get_val()
  %r5 = call i32 @get_val()
  %r6 = call i32 @get_val()
  call void @use_vals(i32 %r4, i32 %r5, i32 %r6)
  ret void
}

declare i32 @get_val()
declare void @use_vals(i32, i32, i32)

; VarArg function. The 4-byte local is the va_list alloca, which is a real
; stack object and must survive.
define void @test_vararg(i32 %a, ...) {
; FP-LABEL: test_vararg:
; FP:       sub_imm sp, sp, 12
; FP:       pushd ra
; FP:       pushd r7
; FP:       mov r7, sp
; FP:       sub_imm sp, sp, 4
; ...
; FP:       mov sp, r7
; FP:       popd r7
; FP:       popd ra
; FP:       add_imm sp, sp, 12
; FP:       mov pc, ra

; NOFP-LABEL: test_vararg:
; NOFP:       sub_imm sp, sp, 12
; NOFP:       pushd ra
; NOFP:       sub_imm sp, sp, 4
; NOFP:       gosub use_val
; NOFP:       add_imm sp, sp, 4
; NOFP:       popd ra
; NOFP:       add_imm sp, sp, 12
; NOFP:       mov pc, ra
entry:
  %ap = alloca ptr, align 4
  call void @llvm.va_start(ptr %ap)
  %val = va_arg ptr %ap, i32
  call void @use_val(i32 %val)
  call void @llvm.va_end(ptr %ap)
  ret void
}

declare void @llvm.va_start(ptr)
declare void @llvm.va_end(ptr)
declare void @use_val(i32)
