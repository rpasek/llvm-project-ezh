; RUN: llc -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O3 < %s | FileCheck %s

; LSR prefers post-increment addressing (getPreferredAddressingMode =
; AMK_PostIndexed with an honest isLegalAddressingMode), so a loop that
; also keeps a counter no longer shares one induction variable between the
; address and the count -- the pointer bump folds into ldrb_post instead
; of paying a reg-offset load plus a separate increment.
define i32 @my_strlen(ptr %s) {
; CHECK-LABEL: my_strlen:
; CHECK:       ldrb_post r{{[0-9]}}, r{{[0-9]}}, 1
; CHECK-NOT:   ldr_regb
entry:
  br label %loop
loop:
  %p = phi ptr [ %s, %entry ], [ %p2, %loop ]
  %n = phi i32 [ 0, %entry ], [ %n2, %loop ]
  %c = load i8, ptr %p
  %p2 = getelementptr i8, ptr %p, i32 1
  %n2 = add i32 %n, 1
  %z = icmp eq i8 %c, 0
  br i1 %z, label %done, label %loop
done:
  ret i32 %n
}
