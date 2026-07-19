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
// CHECK-LABEL: @rgpo(
// CHECK: call i32 @llvm.ezh.read.gpo()
unsigned rgpo(void) { return __builtin_ezh_read_gpo(); }
// CHECK-LABEL: @wgpo(
// CHECK: call void @llvm.ezh.write.gpo(i32
void wgpo(unsigned v) { __builtin_ezh_write_gpo(v); }
// CHECK-LABEL: @wgpd(
// CHECK: call void @llvm.ezh.write.gpd(i32
void wgpd(unsigned m) { __builtin_ezh_write_gpd(m); }
