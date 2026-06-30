// RUN: %clang_cc1 -triple ezh-none-elf -emit-llvm -o - %s | FileCheck %s
//
// The __builtin_ezh_* GPIO builtins lower to the ezh intrinsics automatically
// through the ClangBuiltin association (no target-specific CodeGen).

// CHECK-LABEL: @sample(
// CHECK: call i32 @llvm.ezh.read.gpi()
unsigned sample(void) { return __builtin_ezh_read_gpi(); }

// CHECK-LABEL: @sample_d(
// CHECK: call i32 @llvm.ezh.read.gpd()
unsigned sample_d(void) { return __builtin_ezh_read_gpd(); }

// CHECK-LABEL: @pull(
// CHECK: call void @llvm.ezh.gpd.drive.low(i32 5)
void pull(void) { __builtin_ezh_gpd_drive_low(5); }

// CHECK-LABEL: @rel(
// CHECK: call void @llvm.ezh.gpd.release(i32 5)
void rel(void) { __builtin_ezh_gpd_release(5); }
