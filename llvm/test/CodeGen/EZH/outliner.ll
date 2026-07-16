; RUN: llc -verify-machineinstrs -mtriple=ezh-none-elf -O2 -enable-machine-outliner=always < %s | FileCheck %s

; The MachineOutliner extracts the identical MIX kernel shared by the
; framed f* functions into OUTLINED_FUNCTION_0 (called via gosub, returning
; through the RA link, body pure ALU + one return, no branch/call/pc-load).
; The frameless leaf sharing the same kernel is NOT outlined: RA holds the
; live return address the outline call would clobber.

; Emitted order: the f* callers first, the frameless leaf, then the
; outlined function last.
; CHECK-LABEL: f0:
; CHECK: gosub OUTLINED_FUNCTION_0
; CHECK-LABEL: leaf:
; CHECK-NOT: gosub OUTLINED_FUNCTION
; CHECK-LABEL: OUTLINED_FUNCTION_0:
; CHECK-NOT: gosub
; CHECK-NOT: ldr {{[^,]*}}, pc,
; CHECK: mov pc, ra

; ModuleID = 'outlmin.c'
source_filename = "outlmin.c"
target datalayout = "e-m:e-p:32:32-i64:32-f64:32-n32-S32"
target triple = "ezh-unknown-none-elf"

; Function Attrs: minsize nounwind optsize
define dso_local void @f0(i32 noundef %0, i32 noundef %1, i32 noundef %2, i32 noundef %3) local_unnamed_addr #0 {
  %5 = xor i32 %1, %0
  %6 = and i32 %5, %2
  %7 = or i32 %6, %3
  %8 = shl i32 %7, 3
  %9 = lshr i32 %7, 5
  %10 = xor i32 %8, %9
  %11 = sub i32 %0, %1
  %12 = add i32 %11, %10
  %13 = or i32 %12, %2
  %14 = and i32 %13, %3
  %15 = shl i32 %14, 7
  %16 = lshr i32 %14, 9
  %17 = or i32 %15, %16
  %18 = xor i32 %17, %0
  %19 = shl i32 %2, 2
  %20 = sub i32 %19, %3
  %21 = add i32 %20, %18
  %22 = shl i32 %1, 1
  %23 = xor i32 %21, %22
  tail call void @sink(i32 noundef %23) #3
  ret void
}

; Function Attrs: minsize optsize
declare dso_local void @sink(i32 noundef) local_unnamed_addr #1

; Function Attrs: minsize nounwind optsize
define dso_local void @f1(i32 noundef %0, i32 noundef %1, i32 noundef %2, i32 noundef %3) local_unnamed_addr #0 {
  %5 = xor i32 %1, %0
  %6 = and i32 %5, %2
  %7 = or i32 %6, %3
  %8 = shl i32 %7, 3
  %9 = lshr i32 %7, 5
  %10 = xor i32 %8, %9
  %11 = sub i32 %0, %1
  %12 = add i32 %11, %10
  %13 = or i32 %12, %2
  %14 = and i32 %13, %3
  %15 = shl i32 %14, 7
  %16 = lshr i32 %14, 9
  %17 = or i32 %15, %16
  %18 = xor i32 %17, %0
  %19 = shl i32 %2, 2
  %20 = sub i32 %19, %3
  %21 = add i32 %20, %18
  %22 = shl i32 %1, 1
  %23 = xor i32 %21, %22
  tail call void @sink(i32 noundef %23) #3
  ret void
}

; Function Attrs: minsize nounwind optsize
define dso_local void @f2(i32 noundef %0, i32 noundef %1, i32 noundef %2, i32 noundef %3) local_unnamed_addr #0 {
  %5 = xor i32 %1, %0
  %6 = and i32 %5, %2
  %7 = or i32 %6, %3
  %8 = shl i32 %7, 3
  %9 = lshr i32 %7, 5
  %10 = xor i32 %8, %9
  %11 = sub i32 %0, %1
  %12 = add i32 %11, %10
  %13 = or i32 %12, %2
  %14 = and i32 %13, %3
  %15 = shl i32 %14, 7
  %16 = lshr i32 %14, 9
  %17 = or i32 %15, %16
  %18 = xor i32 %17, %0
  %19 = shl i32 %2, 2
  %20 = sub i32 %19, %3
  %21 = add i32 %20, %18
  %22 = shl i32 %1, 1
  %23 = xor i32 %21, %22
  tail call void @sink(i32 noundef %23) #3
  ret void
}

; Function Attrs: minsize nounwind optsize
define dso_local void @f3(i32 noundef %0, i32 noundef %1, i32 noundef %2, i32 noundef %3) local_unnamed_addr #0 {
  %5 = xor i32 %1, %0
  %6 = and i32 %5, %2
  %7 = or i32 %6, %3
  %8 = shl i32 %7, 3
  %9 = lshr i32 %7, 5
  %10 = xor i32 %8, %9
  %11 = sub i32 %0, %1
  %12 = add i32 %11, %10
  %13 = or i32 %12, %2
  %14 = and i32 %13, %3
  %15 = shl i32 %14, 7
  %16 = lshr i32 %14, 9
  %17 = or i32 %15, %16
  %18 = xor i32 %17, %0
  %19 = shl i32 %2, 2
  %20 = sub i32 %19, %3
  %21 = add i32 %20, %18
  %22 = shl i32 %1, 1
  %23 = xor i32 %21, %22
  tail call void @sink(i32 noundef %23) #3
  ret void
}

; Function Attrs: minsize nounwind optsize
define dso_local void @f4(i32 noundef %0, i32 noundef %1, i32 noundef %2, i32 noundef %3) local_unnamed_addr #0 {
  %5 = xor i32 %1, %0
  %6 = and i32 %5, %2
  %7 = or i32 %6, %3
  %8 = shl i32 %7, 3
  %9 = lshr i32 %7, 5
  %10 = xor i32 %8, %9
  %11 = sub i32 %0, %1
  %12 = add i32 %11, %10
  %13 = or i32 %12, %2
  %14 = and i32 %13, %3
  %15 = shl i32 %14, 7
  %16 = lshr i32 %14, 9
  %17 = or i32 %15, %16
  %18 = xor i32 %17, %0
  %19 = shl i32 %2, 2
  %20 = sub i32 %19, %3
  %21 = add i32 %20, %18
  %22 = shl i32 %1, 1
  %23 = xor i32 %21, %22
  tail call void @sink(i32 noundef %23) #3
  ret void
}

; Function Attrs: minsize nounwind optsize
define dso_local void @f5(i32 noundef %0, i32 noundef %1, i32 noundef %2, i32 noundef %3) local_unnamed_addr #0 {
  %5 = xor i32 %1, %0
  %6 = and i32 %5, %2
  %7 = or i32 %6, %3
  %8 = shl i32 %7, 3
  %9 = lshr i32 %7, 5
  %10 = xor i32 %8, %9
  %11 = sub i32 %0, %1
  %12 = add i32 %11, %10
  %13 = or i32 %12, %2
  %14 = and i32 %13, %3
  %15 = shl i32 %14, 7
  %16 = lshr i32 %14, 9
  %17 = or i32 %15, %16
  %18 = xor i32 %17, %0
  %19 = shl i32 %2, 2
  %20 = sub i32 %19, %3
  %21 = add i32 %20, %18
  %22 = shl i32 %1, 1
  %23 = xor i32 %21, %22
  tail call void @sink(i32 noundef %23) #3
  ret void
}

; Function Attrs: minsize mustprogress nofree norecurse nosync nounwind optsize willreturn memory(none)
define dso_local i32 @leaf(i32 noundef %0, i32 noundef %1, i32 noundef %2, i32 noundef %3) local_unnamed_addr #2 {
  %5 = xor i32 %1, %0
  %6 = and i32 %5, %2
  %7 = or i32 %6, %3
  %8 = shl i32 %7, 3
  %9 = lshr i32 %7, 5
  %10 = xor i32 %8, %9
  %11 = sub i32 %0, %1
  %12 = add i32 %11, %10
  %13 = or i32 %12, %2
  %14 = and i32 %13, %3
  %15 = shl i32 %14, 7
  %16 = lshr i32 %14, 9
  %17 = or i32 %15, %16
  %18 = xor i32 %17, %0
  %19 = shl i32 %2, 2
  %20 = sub i32 %19, %3
  %21 = add i32 %20, %18
  %22 = shl i32 %1, 1
  %23 = xor i32 %21, %22
  ret i32 %23
}

attributes #0 = { minsize nounwind optsize "no-builtins" "no-trapping-math"="true" "stack-protector-buffer-size"="8" }
attributes #1 = { minsize optsize "no-builtins" "no-trapping-math"="true" "stack-protector-buffer-size"="8" }
attributes #2 = { minsize mustprogress nofree norecurse nosync nounwind optsize willreturn memory(none) "no-builtins" "no-trapping-math"="true" "stack-protector-buffer-size"="8" }
attributes #3 = { minsize nobuiltin nounwind optsize "no-builtins" }

!llvm.ident = !{!0}
!llvm.errno.tbaa = !{!1}

!0 = !{!"clang version 23.0.0git (https://github.com/bogdan-petru/llvm-project-ezh.git 9ea84a28d62bb23aa0329da16d6673c12892cfda)"}
!1 = !{!2, !2, i64 0}
!2 = !{!"int", !3, i64 0}
!3 = !{!"omnipotent char", !4, i64 0}
!4 = !{!"Simple C/C++ TBAA"}
