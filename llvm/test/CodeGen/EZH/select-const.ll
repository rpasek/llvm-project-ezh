; RUN: llc -verify-machineinstrs -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O3 < %s | FileCheck %s
; RUN: llc -verify-machineinstrs -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O3 -stop-after=if-converter < %s | FileCheck %s --check-prefix=MIR

; A select of an immediate folds the materialization into the predicated
; slot: the if-converter produces load_imm_cc dst, K instead of
; materialize-then-mov_cc, and the standalone load disappears.
;
; The MIR prefix pins the internal opcode split: predicating a materializer
; must switch it to its *_CC twin (which carries hasSideEffects=1 and is not
; rematerializable), never leave a predicated instance on the base opcode
; whose descriptor is movable. The asm output is identical either way (the
; twins share the encoding), so only a -stop-after=if-converter check can
; catch a regression in PredicateInstruction's opcode rewrite.

define i32 @pick(i32 %c, i32 %x) {
; CHECK-LABEL: pick:
; CHECK:       load_imm r0, 7
; CHECK-NEXT:  load_imm_ze r0, 42
; CHECK-NEXT:  mov pc, ra
; CHECK-NOT:   mov_
; MIR-LABEL: name: pick
; The unconditional materialization stays on the base opcode (EU predicate)...
; MIR: LOAD_IMM 7, 0
; ...and the predicated one must be the _CC twin, not a base LOAD_IMM with a
; non-EU predicate operand.
; MIR-NOT: LOAD_IMM 42
; MIR: LOAD_IMM_CC 42, 1
  %t = icmp eq i32 %c, 0
  %r = select i1 %t, i32 42, i32 7
  ret i32 %r
}

; Only the false side is a constant: the sides swap and the condition
; inverts, so the fold still lands.
define i32 @false_const(i32 %c, i32 %x) {
; CHECK-LABEL: false_const:
; CHECK:       load_imm_nz r{{[0-9]}}, 9
; MIR-LABEL: name: false_const
; MIR-NOT: LOAD_IMM 9
; MIR: LOAD_IMM_CC 9, 2
  %t = icmp eq i32 %c, 0
  %r = select i1 %t, i32 %x, i32 9
  ret i32 %r
}

; Inverted-immediate materializations (load_simmn) fold the same way.
define i32 @mask_or_val(i32 %c, i32 %y) {
; CHECK-LABEL: mask_or_val:
; CHECK:       load_simmn_nz r{{[0-9]}}, -65536
; MIR-LABEL: name: mask_or_val
; MIR-NOT: LOAD_SIMMN -1
; MIR: LOAD_SIMMN_CC -1, 16, 2
  %t = icmp eq i32 %c, 0
  %r = select i1 %t, i32 %y, i32 65535
  ret i32 %r
}
