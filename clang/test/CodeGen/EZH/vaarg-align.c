// RUN: %clang_cc1 -triple ezh-none-elf -emit-llvm %s -o - | FileCheck %s
//
// The EZH stack is only 4-byte aligned (datalayout S32) and CC_EZH packs
// byval arguments at 4, so va_arg must never round the argument pointer up
// to a higher alignment: doing so skips a real argument word whenever the
// caller's SP sits at 4 mod 8, which depends on nothing more stable than
// the caller's register pressure (found on silicon as gcc-torture pr38151
// failing only when an extra callee-saved push changed the SP parity).

struct Aligned {
  unsigned a;
  _Complex int b;
  struct {
  } __attribute__((aligned)) c;
};

// CHECK-LABEL: @read_aligned(
// CHECK-NOT: and i32 {{.*}}, -8
// CHECK-NOT: and i32 {{.*}}, -16
// CHECK: call void @llvm.memcpy.p0.p0.i32(ptr align 16 %{{.*}}, ptr align 4 %{{.*}}, i32 16
struct Aligned read_aligned(__builtin_va_list *ap) {
  return __builtin_va_arg(*ap, struct Aligned);
}
