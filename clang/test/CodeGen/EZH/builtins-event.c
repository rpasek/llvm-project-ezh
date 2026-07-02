// RUN: %clang_cc1 -triple ezh-none-elf -emit-llvm -o - %s | FileCheck %s
//
// The EZH event/control-fabric builtins lower to their ezh intrinsics through
// the generic ClangBuiltin path (no target-specific CodeGen).

// CHECK-LABEL: @rcfm(
// CHECK: call i32 @llvm.ezh.read.cfm()
unsigned rcfm(void) { return __builtin_ezh_read_cfm(); }
// CHECK-LABEL: @wcfm(
// CHECK: call void @llvm.ezh.write.cfm(i32
void wcfm(unsigned x) { __builtin_ezh_write_cfm(x); }
// CHECK-LABEL: @rcfs(
// CHECK: call i32 @llvm.ezh.read.cfs()
unsigned rcfs(void) { return __builtin_ezh_read_cfs(); }
// CHECK-LABEL: @wcfs(
// CHECK: call void @llvm.ezh.write.cfs(i32
void wcfs(unsigned x) { __builtin_ezh_write_cfs(x); }
// CHECK-LABEL: @hold(
// CHECK: call void @llvm.ezh.hold()
void hold(void) { __builtin_ezh_hold(); }
// CHECK-LABEL: @it(
// CHECK: call void @llvm.ezh.int.trigger(i32 42)
void it(void) { __builtin_ezh_int_trigger(42); }
// CHECK-LABEL: @wfb(
// CHECK: call void @llvm.ezh.wait.for.beat()
void wfb(void) { __builtin_ezh_wait_for_beat(); }
// CHECK-LABEL: @satb(
// CHECK: call void @llvm.ezh.synch.all.to.beat(i32 1)
void satb(void) { __builtin_ezh_synch_all_to_beat(1); }
// CHECK-LABEL: @hr(
// CHECK: call void @llvm.ezh.heart.rythm(i32
void hr(unsigned x) { __builtin_ezh_heart_rythm(x); }
// CHECK-LABEL: @hri(
// CHECK: call void @llvm.ezh.heart.rythm.imm(i32 1000)
void hri(void) { __builtin_ezh_heart_rythm_imm(1000); }
// CHECK-LABEL: @mod(
// CHECK: call void @llvm.ezh.modify.gpo.byte(i32 255, i32 0, i32 15)
void mod(void) { __builtin_ezh_modify_gpo_byte(255, 0, 15); }
// CHECK-LABEL: @tl(
// CHECK: call void @llvm.ezh.tight.loop(ptr %{{.*}}, i32 %{{.*}})
void tl(void *rend, unsigned rcount) { __builtin_ezh_tight_loop(rend, rcount); }
// CHECK-LABEL: @cbs(
// CHECK: call void @llvm.ezh.cfm.bset(i32 3)
void cbs(void) { __builtin_ezh_cfm_bset(3); }
// CHECK-LABEL: @cbc(
// CHECK: call void @llvm.ezh.cfm.bclr(i32 0)
void cbc(void) { __builtin_ezh_cfm_bclr(0); }
// CHECK-LABEL: @np(
// CHECK: call void @llvm.ezh.nop()
void np(void) { __builtin_ezh_nop(); }
// CHECK-LABEL: @avh(
// CHECK: call ptr @llvm.ezh.acc.vectored.hold(ptr %{{.*}}, i32 5)
void *avh(void *table) { return __builtin_ezh_acc_vectored_hold(table, 5); }
