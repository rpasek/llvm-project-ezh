; RUN: llc -mtriple=ezh-none-elf -mattr=-bitslice-interrupts -O3 < %s | FileCheck %s

; Basic Register Shift (RLSL)
define i32 @test_rlsl(i32 %a, i32 %b) {
; CHECK-LABEL: test_rlsl:
; CHECK:       rlsl r0, r0, r1
; CHECK:       mov pc, ra
  %res = shl i32 %a, %b
  ret i32 %res
}

; Basic Register Shift Inverted (RLSLN)
define i32 @test_rlsln(i32 %a, i32 %b) {
; CHECK-LABEL: test_rlsln:
; CHECK:       rlsln r0, r0, r1
; CHECK:       mov pc, ra
  %sh = shl i32 %a, %b
  %res = xor i32 %sh, -1
  ret i32 %res
}

; PRE-shift ALU AND (RLSL_AND)
define i32 @test_rlsl_and(i32 %a, i32 %b, i32 %c) {
; CHECK-LABEL: test_rlsl_and:
; CHECK:       rlsl_and r0, r0, r1, r2
; CHECK:       mov pc, ra
  %sh = shl i32 %b, %c
  %res = and i32 %a, %sh
  ret i32 %res
}

; PRE-shift ALU AND Inverted (RLSL_ANDN: ~(a & (b << c)))
define i32 @test_rlsl_andn(i32 %a, i32 %b, i32 %c) {
; CHECK-LABEL: test_rlsl_andn:
; CHECK:       rlsl_andn r0, r0, r1, r2
; CHECK:       mov pc, ra
  %sh = shl i32 %b, %c
  %and = and i32 %a, %sh
  %res = xor i32 %and, -1
  ret i32 %res
}

; POST-shift ALU AND (AND_RLSL)
define i32 @test_and_rlsl(i32 %a, i32 %b, i32 %c) {
; CHECK-LABEL: test_and_rlsl:
; CHECK:       and_rlsl r0, r0, r1, r2
; CHECK:       mov pc, ra
  %and = and i32 %a, %b
  %res = shl i32 %and, %c
  ret i32 %res
}

; POST-shift ALU AND Inverted (ANDN_RLSL: ~(a & b) << c)
define i32 @test_andn_rlsl(i32 %a, i32 %b, i32 %c) {
; CHECK-LABEL: test_andn_rlsl:
; CHECK:       andn_rlsl r0, r0, r1, r2
; CHECK:       mov pc, ra
  %and = and i32 %a, %b
  %not = xor i32 %and, -1
  %res = shl i32 %not, %c
  ret i32 %res
}

; PRE-shift ALU ADD (RLSL_ADD)
define i32 @test_rlsl_add(i32 %a, i32 %b, i32 %c) {
; CHECK-LABEL: test_rlsl_add:
; CHECK:       rlsl_add r0, r0, r1, r2
; CHECK:       mov pc, ra
  %sh = shl i32 %b, %c
  %res = add i32 %a, %sh
  ret i32 %res
}

; PRE-shift ALU ADD Inverted (RLSL_ADDN: ~(a + (b << c)))
define i32 @test_rlsl_addn(i32 %a, i32 %b, i32 %c) {
; CHECK-LABEL: test_rlsl_addn:
; CHECK:       rlsl_addn r0, r0, r1, r2
; CHECK:       mov pc, ra
  %sh = shl i32 %b, %c
  %add = add i32 %a, %sh
  %res = xor i32 %add, -1
  ret i32 %res
}

; PRE-shift ALU SUB (RLSL_SUB: (b << c) - a)
define i32 @test_rlsl_sub(i32 %a, i32 %b, i32 %c) {
; CHECK-LABEL: test_rlsl_sub:
; CHECK:       rlsl_sub r0, r0, r1, r2
; CHECK:       mov pc, ra
  %sh = shl i32 %b, %c
  %res = sub i32 %sh, %a
  ret i32 %res
}

; PRE-shift ALU SUB Inverted (RLSL_SUBN: ~((b << c) - a))
define i32 @test_rlsl_subn(i32 %a, i32 %b, i32 %c) {
; CHECK-LABEL: test_rlsl_subn:
; CHECK:       rlsl_subn r0, r0, r1, r2
; CHECK:       mov pc, ra
  %sh = shl i32 %b, %c
  %sub = sub i32 %sh, %a
  %res = xor i32 %sub, -1
  ret i32 %res
}

; PRE-shift ALU XOR (RLSL_XOR)
define i32 @test_rlsl_xor(i32 %a, i32 %b, i32 %c) {
; CHECK-LABEL: test_rlsl_xor:
; CHECK:       rlsl_xor r0, r0, r1, r2
; CHECK:       mov pc, ra
  %sh = shl i32 %b, %c
  %res = xor i32 %a, %sh
  ret i32 %res
}

; Basic LSR
define i32 @test_rlsr(i32 %a, i32 %b) {
; CHECK-LABEL: test_rlsr:
; CHECK:       rlsr r0, r0, r1
; CHECK:       mov pc, ra
  %res = lshr i32 %a, %b
  ret i32 %res
}

; PRE-shift LSR ADD (RLSR_ADD)
define i32 @test_rlsr_add(i32 %a, i32 %b, i32 %c) {
; CHECK-LABEL: test_rlsr_add:
; CHECK:       rlsr_add r0, r0, r1, r2
; CHECK:       mov pc, ra
  %sh = lshr i32 %b, %c
  %res = add i32 %a, %sh
  ret i32 %res
}

; PRE-shift LSR ADD Inverted (RLSR_ADDN: ~(a + (b lsr c)))
define i32 @test_rlsr_addn(i32 %a, i32 %b, i32 %c) {
; CHECK-LABEL: test_rlsr_addn:
; CHECK:       rlsr_addn r0, r0, r1, r2
; CHECK:       mov pc, ra
  %sh = lshr i32 %b, %c
  %add = add i32 %a, %sh
  %res = xor i32 %add, -1
  ret i32 %res
}

; Basic ASR
define i32 @test_rasr(i32 %a, i32 %b) {
; CHECK-LABEL: test_rasr:
; CHECK:       rasr r0, r0, r1
; CHECK:       mov pc, ra
  %res = ashr i32 %a, %b
  ret i32 %res
}

; PRE-shift ASR SUB (RASR_SUB: (b asr c) - a)
define i32 @test_rasr_sub(i32 %a, i32 %b, i32 %c) {
; CHECK-LABEL: test_rasr_sub:
; CHECK:       rasr_sub r0, r0, r1, r2
; CHECK:       mov pc, ra
  %sh = ashr i32 %b, %c
  %res = sub i32 %sh, %a
  ret i32 %res
}

; Basic ROR
define i32 @test_rror(i32 %a, i32 %b) {
; CHECK-LABEL: test_rror:
; CHECK:       rror r0, r0, r1
; CHECK:       mov pc, ra
  %sub = sub i32 32, %b
  %shl = shl i32 %a, %sub
  %lshr = lshr i32 %a, %b
  %res = or i32 %shl, %lshr
  ret i32 %res
}

; PRE-shift ROR XOR (RROR_XOR)
define i32 @test_rror_xor(i32 %a, i32 %b, i32 %c) {
; CHECK-LABEL: test_rror_xor:
; CHECK:       rror_xor r0, r0, r1, r2
; CHECK:       mov pc, ra
  %sub = sub i32 32, %c
  %shl = shl i32 %b, %sub
  %lshr = lshr i32 %b, %c
  %ror = or i32 %shl, %lshr
  %res = xor i32 %a, %ror
  ret i32 %res
}
