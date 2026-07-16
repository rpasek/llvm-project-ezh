# EZH codegen optimization backlog

Confirmed and ranked by an adversarial discovery sweep over the gcc-torture
corpus and the firmware demos (every entry was independently reproduced
with the built compiler before ranking; rank = payoff/effort, 10 best).
Items already implemented are marked DONE with their commit.

## [9] [DONE 5dfec349] Post-increment addressing not used when the loop also wants the index value: LSR shares one induction variable and pays mov + reg-offset load per iteration instead of ldrb_post

Estimated win: 1-2 instructions per loop *iteration* (runtime win in exactly the byte-pump loops EZH exists for); 4->5 instructions is a 25% hot-loop penalty in the strlen shape; ~150 affected loops in the corpus.

Repro:
```c
int my_strlen(const char *s) {
  int n = 0;
  while (*s++) n++;
  return n;
}
```

Fix: /Users/foxy/Downloads/llvm-ezh-port/llvm/lib/Target/EZH/EZHTargetTransformInfo.h: override getPreferredAddressingMode to return TTI::AMK_PostIndexed (ARM does this for Thumb-class cores), and implement TargetLowering::isLegalAddressingMode in EZHISelLowering.cpp with the true EZH ranges (word +/-508, byte +/-127, reg+reg with no offset, no scale) so LSR's cost model stops treating the mov+lsl+reg-offset form as free.

## [9] [DONE] Signed compares against 0/±1 emit the full sign-bit-bias sequence (btog + INT_MIN materialization + subs + carry test) instead of a single sub_imms with the PO/NE/AZ/ZB condition

Estimated win: 8 bytes (2 insns) per site, plus a 4-byte pool entry and a slow pc-relative load for the ±1 cases, plus reduced register pressure (in the repro it deletes a pushd/popd r4 pair = 8 more bytes and 2 stack ops). ~224 sites / 848 files.

Repro:
```c
void zero(int *p, int n) { for (int i = 0; i < n; i++) p[i] = 0; }   // guard is 'n < 1'
int guard(int st, int *p) { if (st < 0) return -2; *p = st; return 0; }   // 'st < 0'
```

Fix: llvm/lib/Target/EZH/EZHISelLowering.cpp, preprocessComparison() (~line 598): (a) if LHS is constant 0, swap operands and flip the condition; (b) canonicalize RHS constant: SETLT x,1 -> SETLE x,0; SETGE x,1 -> SETGT x,0; SETGT x,-1 -> SETGE x,0; SETLE x,-1 -> SETLT x,0; (c) treat RHS == constant 0 as safe (skip the BTOG/bias path) — subtraction of 0 cannot overflow. IntCCToEZHCC and the PseudoCMP custom inserter already map the signed CCs to PO/NE/AZ/ZB. Bonus: the resulting sub_imms x,0 becomes eligible for the existing EZHCompareFusion S-twin fold when x comes from an ALU producer.

## [9] [DONE] LOAD_SIMMN never selected: all 2^n-1 masks and trailing-ones constants go to the constant pool

Estimated win: 4 bytes + 1 SRAM load per occurrence (8B ldr+slot -> 4B ALU insn), ~2.3KB code+pool across the corpus; bonus: fewer constant-island slots so fewer island clones (1466 cloned slots observed) and branch-around gotos

Repro:
```c
unsigned zext16(unsigned x) { return x & 0xFFFF; }
unsigned mask7ff(unsigned x) { return x & 0x7FF; }
int intmax(void) { return 0x7FFFFFFF; }
// clang -target ezh-none-elf -Os (same at -O2)
```

Fix: EZHISelLowering.cpp LowerConstant (line 898): after the existing LOAD_SIMM check (isInt<11>(SVal>>TZ), line 911), run the identical check on ~UVal and emit DAG.getMachineNode(EZH::LOAD_SIMMN, DL, MVT::i32, {HiOfNot, ShiftOfNot, Pred}). Also teach the RecoverConstant lambda in the signed-compare bias combine (EZHISelLowering.cpp:649) to recover values from LOAD_SIMMN nodes so the C^0x80000000 fold keeps applying. Side fix while there: AsmParser cvtShiftedImm (EZHAsmParser.cpp:247) silently truncates unencodable values (`load_simmn r0, 65535` assembles to imm=-1,sh=0 = wrong value) - add a post-split isInt<11> range check. Validate the LOAD_SIMMN encoding once against ezhdis since only the SIMM form is silicon-proven.

## [9] [DONE 222f8f47] i64 add/sub/neg re-derive the carry with subs+mov_ca instead of using adds/adc -- the adde/sube patterns in the td are dead code

Estimated win: 28-44 bytes and ~10 cycles per i64 add/sub (9-13 insns -> 2-3), plus 2 callee-saved registers freed per site on an 8-register machine (kills the pushd/popd pairs and spill pressure around them).

Repro:
```c
unsigned long long add64(unsigned long long a, unsigned long long b) { return a + b; }  (also: a-b, -a, a+1, __builtin_add_overflow)  // -Os or -O2, same result
```

Fix: EZHInstrInfo.td:770-771 already attaches adde/sube patterns to ADC/SBC, but they can never match: ADDC/ADDE/SUBC/SUBE default to Expand (TargetLoweringBase.cpp:1173) and EZHISelLowering.cpp never overrides, so ExpandIntRes_ADDSUB (LegalizeIntegerTypes.cpp:3886) falls through to the boolean-SETCC expansion. Preferred fix: implement the modern path the legalizer checks FIRST (LegalizeIntegerTypes.cpp:3861) -- setOperationAction(ISD::{UADDO,USUBO,UADDO_CARRY,USUBO_CARRY}, MVT::i32, Custom) in the EZHTargetLowering ctor and lower to new glued EZHISD::ADDS/ADC/SUBS/SBC nodes selected to the existing ADDS/ADCS/SUBS/SBCS instructions (ARM does exactly this with ARMISD::ADDC/ADDE). Quick-and-dirty alternative: mark ISD::{ADDC,ADDE,SUBC,SUBE} i32 Legal and add addc->ADDS / subc->SUBS Pats; note adde/sube must map to the flag-SETTING adcs/sbcs (explicit Pat; the ALUOps multiclass only puts patterns on the non-S forms, EZHInstrInfo.td:451-455) or >64-bit chains miscompile.

## [9] [DONE] LowerGlobalAddress pools global+offset per offset: s, s+4, s+8 each get a distinct pool entry and pc-load

Estimated win: 8 bytes per extra offset occurrence (pool entry + pc-load); ~7KB corpus-wide; localized ISel change

Repro:
```c
struct S { int x, y, z; } s;
int a;
void t(void) { s.y = s.x; s.z = a; }
```

Fix: EZHISelLowering.cpp LowerGlobalAddress (line ~932): when GV->getOffset() != 0 and isInt<11>(Offset), emit LOAD_CONSTANT of the bare GV and wrap it in DAG.getNode(ISD::ADD, ..., getConstant(Offset)). SelectionDAG CSEs the bare-GV node across all offsets in a block, and the existing MemOps patterns (EZHInstrInfo.td:813-831, (load (add i32:$Rn, imms8_word:$Offset))) fold the add into the ldr/str offset; non-memory uses select ADD_IMM (imms11). Keep the pooled GV+off form only as fallback for offsets outside imm range.

## [9] [DONE 2341fef2] LOAD_CONSTANT has no MMO and is not rematerializable, so MachineCSE never merges cross-block reloads of the same pool entry

Estimated win: 4 bytes per eliminated reload; up to 16KB corpus (35% of all pool loads), ~1.4KB across the 24 firmware demos; small targeted change

Repro:
```c
extern volatile unsigned REG;
void pulse(int n) {
  REG = 1;
  if (n > 0) REG = 2;
  REG = 3;
}
```

Fix: Two parts, mirroring ARM's constant-pool pseudos: (1) in EZHISelLowering (LowerGlobalAddress/LowerConstant/LowerConstantPool/LowerJumpTable/LowerBlockAddress) attach a MachineMemOperand (PseudoSourceValue ConstantPool, MOLoad|MOInvariant|MODereferenceable, 4 bytes) via DAG.setNodeMemRefs on the LOAD_CONSTANT MachineSDNode -- without it MachineCSE::isCSECandidate rejects the instr because mayLoad && !isDereferenceableInvariantLoad. (2) mark LOAD_CONSTANT/LOAD_CONSTANT_COND (EZHInstrInfo.td:560-562) isReMaterializable = 1 so regalloc can undo the extended live range by rematerializing instead of spilling when pressure is tight on the 8-reg file.

## [9] [DONE] mayBeEmittedAsTailCall not overridden: conditionally-reached tail calls stay gosub and force an otherwise-unneeded RA save

Estimated win: 4-12 bytes per affected function (drop pushd ra + popd->mov, gosub+goto -> goto); ~1.5KB corpus; one-line hook

Repro:
```c
extern void h(void);
void cond_tail(int x) { if (x) h(); }
```

Fix: EZHISelLowering: override TargetLowering::mayBeEmittedAsTailCall (TargetLowering.h:5141 default returns false) to return true for CI->isTailCall() under CallingConv::C, like RISCVISelLowering.cpp:25569. CodeGenPrepare::dupRetToEnableTailCallOpts then duplicates the ret into the calling block and the existing TCRETURN path (EZHInstrInfo.td:1031) takes over; LowerCall's eligibility checks (stack-arg-free etc., EZHISelLowering.cpp:1147-1159) already guard correctness.

## [8] [DONE ed6c60f1] Flag-to-0/1 materialization uses load_imm 0 + load_imm 1 + predicated reg-mov instead of a predicated load_imm — burns an instruction AND a register (often a callee-saved push/pop pair)

Estimated win: 1-3 insns (4-12 bytes) per site, plus one register freed — in the repros that turns into removing a pushd/popd pair (8 more bytes + 4 SRAM accesses); addll drops from 12 insns to 5

Repro:
```c
int lt(int a, int b) { return a < b; }
long long addll(long long a, long long b) { return a + b; }
```

Fix: Same pipeline gap as finding 1 (predicated copies born in if-converter, after last machine-cp). Peephole in the same pre-emit pass: (a) `load_imm rY,C … mov_cc rX,rY` with rY dead and C in imms11 → `load_imm_cc rX,C` (kills the load_imm too when rY has no other use); (b) `mov_cc rX,rY` where rY=load_imm 1 and result feeds `add rZ,rW,rX` → `add_imm_cc rZ,rZ,1` for the carry shape. Deeper alternative for the carry case: custom-lower UADDO/ADDCARRY (EZHISelLowering.cpp currently legalizes overflow to select_cc of constants 1/0, see comment near line 63) straight to subs + predicated add_imm.

## [8] [DONE] GlobalAddress+offset mints a separate constant-pool entry and pc-relative load per distinct (global, offset) instead of one shared base + folded immediate offset

Estimated win: ~8 bytes (4 pool + 4 code) plus one serial SRAM load per redundant entry; >=2,054 foldable occurrences in the corpus (~16 KB). touch() alone: 36 bytes -> 24 bytes and 5 pc-loads -> 1.

Repro:
```c
struct S { int a, b, c, d, e; };
struct S g;
void touch(void) { g.a = 1; g.b = 2; g.c = 3; g.d = 4; g.e = 5; }
// clang -target ezh-none-elf -mno-ezh-bitslice-interrupts -Os -S
```

Fix: /Users/foxy/Downloads/llvm-ezh-port/llvm/lib/Target/EZH/EZHISelLowering.cpp LowerGlobalAddress (line ~932): when GV->getOffset() != 0, emit the pool entry for the bare GlobalValue and wrap the LOAD_CONSTANT in an ISD::ADD of the offset; SelectionDAG CSE then shares one base load and the existing (load/store (add Rn, imms8_word)) patterns in EZHInstrInfo.td fold the offset into the memory instruction (verified working: pointer-based p->b already emits ldr rD, rN, 4). Also override TargetLowering::isOffsetFoldingLegal to return false so DAGCombiner stops re-merging (add GA, C) into GA+C. Keep the pooled sym+off form only for offsets outside add_imm's imms12 range.

## [8] [DONE 2341fef2 + side-effect descriptor fix, except ADD_IMM] LOAD_CONSTANT / LOAD_IMM / LOAD_SIMM / ADD_IMM are not marked isReMaterializable, so register pressure spills freshly materialized addresses and constants to the stack instead of recomputing them

Estimated win: Eliminates the spill store (4 bytes code + 1 SRAM store + 4 bytes frame) per occurrence and converts the reload into a cycle-equivalent pool load; >=536 adjacent occurrences in the corpus, 10 in the one-function repro alone.

Repro:
```c
extern volatile int g0,g1,g2,g3,g4,g5,g6,g7,g8,g9;
void f(void) {
  int a=g0,b=g1,c=g2,d=g3,e=g4,x=g5,h=g6,i=g7;
  g8 = a+b+c+d+e+x+h+i;
  g9 = a^b^c^d^e^x^h^i;
  g0=a; g1=b; g2=c; g3=d; g4=e; g5=x; g6=h; g7=i;
}
```

Fix: /Users/foxy/Downloads/llvm-ezh-port/llvm/lib/Target/EZH/EZHInstrInfo.td: add isReMaterializable = 1 (and isAsCheapAsAMove for the ALU forms) to the LoadImmOps/LoadSimmOps multiclasses, the LOAD_CONSTANT pseudo (ARM precedent: tLDRpci is isReMaterializable despite mayLoad), and ADD_IMM (covers sp-relative address materialization; RISCV marks ADDI the same way). If the generic isReallyTriviallyReMaterializable check balks at the SP read or the constant-pool MMO, override it in EZHInstrInfo.cpp.

## [8] [DONE dadf3cd8] Select-of-constants materializes both values then conditionally moves: 'load_imm rA,K; mov_cc rD,rA' never folded into the predicated 'load_imm_cc rD,K'

Estimated win: 4 bytes (1 insn) per site, often 8 with the freed register shuffle; ~391 sites / 848 files (~0.5 per file). The select-feeding-add combine saves 12 bytes (3 insns) per counting-loop body.

Repro:
```c
int setcc_lt(int a, int b) { return a < b; }
int scan(const unsigned char *s, int n) { int c=0; for (int i=0;i<n;i++) if (s[i]==0x7e) c++; return c; }
```

Fix: Post-RA peephole (natural home: the EZHCompareFusion pass file, which already runs at addPreEmitPass, after the IfConverter): for 'mov_cc rD, rA' where rA's unique in-block def is an unpredicated LOAD_IMM/LOAD_SIMM and rA is dead after the mov, rewrite to LOAD_IMM/LOAD_SIMM with the mov's CC into rD at the mov's position and erase the load (load_imm writes no flags, so sinking past subs is safe). Extension for the scan shape: a DAGCombine on (add x, (EZHISD::SELECT_CC ..., 1, 0)) emitting a predicated ADD_IMM, which removes all three helper instructions in counting loops.

## [8] LOAD_CONSTANT pseudo is not predicable, so any triangle/diamond that touches a global address or pool constant fails if-conversion — while the identical code with a pointer argument predicates fully

Estimated win: 4-8 bytes per site (removed goto, merged blocks; +4 more in bitslice mode for the dropped gotol_bs) and one taken branch per dynamic execution; 49+ sites / 848 files, disproportionately common in real firmware.

Repro:
```c
int g; extern int h;
void gstore(int x) { if (x) g = 5; }
void gdia(int x) { if (x) g = 5; else h = 7; }
// contrast: void tstore(int x, int *p) { if (x) *p = 5; }  -> fully predicated, no branch
```

Fix: llvm/lib/Target/EZH/EZHInstrInfo.cpp: make isPredicable() return true for EZH::LOAD_CONSTANT, and in PredicateInstruction() handle it by setDesc(LOAD_CONSTANT_COND), appending the predicate immediate and the implicit dest-reg use (same pattern as the existing pred-operand path at line 333). ~15 lines; the printer and constant-island pass already handle the _COND form end-to-end.

## [8] [DONE f44c4db5] GlobalAddress+offset baked into pool entries: one pool slot + one pool load per struct field / array element, ldr offset field unused

Estimated win: 8 bytes + 1 SRAM load per folded slot (kills both the extra `ldr rX, pc, .LCPI` and the 4-byte slot); ~27KB across the corpus, typically 12-24B per struct-heavy function

Repro:
```c
struct S { int a, b, c, d; };
extern struct S gs;
int sum_fields(void) { return gs.a + gs.b + gs.c + gs.d; }
```

Fix: EZHISelLowering.cpp LowerGlobalAddress (line 932): when GV->getOffset() != 0, stop wrapping (GV,offset) in a dedicated EZHConstantPoolValue; instead emit (ISD::ADD (LOAD_CONSTANT GV+0), offset). The existing load/store addressing-mode fold absorbs the add into the ldr/str offset field, and SelectionDAG getMachineNode CSE merges the identical LOAD_CONSTANT base nodes within a block (all take the entry-node chain). For cross-BB reuse ensure LOAD_CONSTANT's MMO is marked constant/invariant so MachineCSE accepts it. Guard: keep the combined pool entry when the offset exceeds the mem-offset range or the address escapes non-memory uses, to avoid a net +4B on lone address takes.

## [8] No 2-insn load_simm+add_imm fallback: pool load used even when a shift-immediate plus 12-bit add builds the constant

Estimated win: size-neutral (8B vs 8B) but removes 1 SRAM load per occurrence and shrinks constant islands: fewer cloned slots in big functions, fewer branch-around gotos and .p2align pads; strictly better on the cycle count of every execution

Repro:
```c
unsigned f(void) { return 0x80000001u; }  // INT_MIN+1, ubiquitous in overflow tests
unsigned g(void) { return 12345; }
```

Fix: Extend the cost model in EZHISelLowering.cpp LowerConstant (line 898): before falling back to LOAD_CONSTANT, test whether UVal-d (d = sign-extended low 12 bits, i.e. d = (int32_t)(UVal<<20)>>20) passes the LOAD_SIMM or new LOAD_SIMMN check; if so emit the machine-node pair LOAD_SIMM(base) feeding ADD_IMM (EZHInstALUI12, imms12). Two glued machine nodes from LowerConstant, or lower to (ISD::ADD Custom-const, d) and let ISel patterns pick add_imm. Keep pool fallback for the truly hard 58% (0x41C64E6D-style multipliers).

## [8] [DONE 69fadd44] Variable-amount i64 shifts are libcalls (__ashldi3/__lshrdi3/__ashrdi3) even at -O2 because SHL/SRA/SRL_PARTS are set to Expand

Estimated win: Per site at -O2: removes gosub+ra save and the ~30-60 cycle helper for ~8 inline insns; ~5x cycle win. Neutral-to-negative on size, so gate on OptForSpeed.

Repro:
```c
unsigned long long shlv(unsigned long long x, unsigned n) { return x << n; }   // -O2
```

Fix: EZHISelLowering.cpp:301-303 sets SHL_PARTS/SRA_PARTS/SRL_PARTS to Expand, which makes the type legalizer fall back to the runtime call. Change to Custom and implement LowerShiftLeftParts/LowerShiftRightParts (clone ARM's, using EZH predicated moves instead of branches). Optionally keep the libcall at MinSize since the call site is 3 insns; the win is -O2 speed.

## [8] [DONE 4cd99235] GlobalMerge pass not in pipeline: every small global costs its own pool entry + pc-relative load per function

Estimated win: 8 bytes per merged global per function (4B pool entry + 4B pc-load); ~14KB / ~4% over the 361KB corpus; near-trivial effort since GlobalMerge is a stock pass

Repro:
```c
int a, b, c;
void tick(void) { a = b + c; }
// clang -target ezh-none-elf -mno-ezh-bitslice-interrupts -Os -ffreestanding -S
```

Fix: EZHTargetMachine.cpp: EZHPassConfig currently has no addIRPasses override. Add one that calls addPass(createGlobalMergePass(TM, MaxOffset)) exactly like ARMTargetMachine (ARMTargetMachine.cpp:400-415). MaxOffset = 508 to match the word ldr/str immediate reach (use 127 if byte-array access folding matters more). Existing (load (add Rn, imms8)) td patterns fold the anchor offsets automatically.

## [7] Select lowering leaves a predicated-mov + copy-back pair; one predicate inversion deletes the copy

Estimated win: 1 insn / 4 bytes per site; 25% code-size cut on leaf min/max helpers

Repro:
```c
unsigned f(unsigned a, unsigned b) { return a < b ? a : b; }  // -Os or -O2
```

Fix: Root cause: PseudoSELECT_CC (EZHISelLowering.cpp:1594) is expanded to a diamond + PHI; phi-elim makes the copy, if-converter (addPreSched2) predicates it — but per llc -debug-pass=Structure the last machine-cp runs BEFORE postrapseudos and if-converter, so nothing ever cleans predicated copies. Fix A (small): pre-emit peephole (next to EZHCompareFusion in EZHTargetMachine.cpp addPreEmitPass) rewriting `mov_cc A,B ; mov B,A` (A dead, adjacent) to `mov_inv(cc) B,A` using the CC pairs ZE/NZ, CA/NC, PO/NE, ZB/AZ (all forms assemble, verified with llvm-mc round-trip). Fix B (structural, ARM-style): replace the diamond expansion with a tied-operand MOVCC pseudo (dst tied to falsev) so the coalescer folds the copy for free. Also worth implementing EZHInstrInfo::isCopyInstrImpl for always-predicated EZH::MOV so any late copy-cleanup pass can see MOVs at all.

## [7] [PARTIAL 8d4c5674] Libcalls are never tail-called: isUsedByReturnOnly not overridden, so every tail-position __mulsi3/__ashldi3/soft-float call keeps a pushd ra / popd pc frame

Estimated win: 2 insns / 8 bytes + 4 SRAM stack ops per site (pushd ra + popd pc + gosub → goto); pure forwarding wrappers shrink 3x

Repro:
```c
long long shl(long long x, int n) { return x << n; }
```

Fix: Override isUsedByReturnOnly in EZHTargetLowering (/Users/foxy/Downloads/llvm-ezh-port/llvm/lib/Target/EZH/EZHISelLowering.cpp|.h), modeled on RISCV/ARM: walk the node's uses, accept CopyToReg into R0..R3 whose glue/chain feeds the EZH return node, and hand back TCChain. LowerCall's existing eligibility check (line 1147-1160) already accepts direct external-symbol callees with register-only args, so the goto path lights up with no other change.

STATUS (8d4c5674, verified 2026-07-16): PARTIAL — only SINGLE-REGISTER-result libcalls tail-call today. `mul_i32` → `goto __mulsi3` ✓; but soft-float `add_f32` → `pushd ra / gosub __addsf3 / popd pc` and i64-result `div_i64` → `pushd ra / gosub __divdi3 / popd pc` both still keep the frame. The remaining work is multi-register (register-pair) return values: isUsedByReturnOnly must accept the R0:R1 pair (and the soft-float return path) feeding the EZH return node, not just a single CopyToReg into R0. Also note: passes 31/31 host tests and codegen is correct, but the win is ZERO `.text` in the DEFAULT (bitslice-interrupts ON) corpus. In bitslice mode the tail call still becomes a `goto` (opt fires), but the injected `gotol_bs bitslice_handler` poll writes RA, so RA must be saved/restored around it — the frame becomes `pushd ra / gotol_bs / popd ra / goto __mulsi3` (4 insns), the same count as the un-optimized `pushd ra / gotol_bs / gosub / popd pc`. The `goto`-for-`gosub` swap nets zero bytes. The 2-insn/8-byte win is realized only under `-mno-ezh-bitslice-interrupts`, where the whole frame collapses to a bare `goto __mulsi3`.

## [7] Reg-offset store folds a multi-use address add and re-materializes the scaled index with a standalone lsl even though an lsl_add of the same address already exists

Estimated win: 1 instruction (4 bytes) per site plus one register freed; 30 corpus sites.

Repro:
```c
int stackidx(int i) {
  volatile int buf[8];
  buf[i] = 5;
  return buf[i+1];
}
```

Fix: In EZHISelDAGToDAG.cpp, gate the reg-offset fold on profitability: when the address (add) node has other uses that force it into a register anyway (i.e. it will be selected as ADD/lsl_add regardless), select the memop as base+imm0 off that result instead of folding. Cleanest as a small ComplexPattern for the reg-offset forms whose Select checks N->hasOneUse() || allOtherUsesAreFoldableMemops; alternatively a post-ISel peephole that rewrites str_reg/ldr_reg to base+0 when an ALU-shifted twin of the address is in scope.

## [7] Jump-table dispatch clobbers RA as scratch, forcing 'pushd ra' in leaf functions, and the 4-entry minimum-jump-table threshold is below EZH's break-even

Estimated win: (a) 4 bytes + 1 push and N pop-returns converted to non-memory returns per leaf switch function; (b) ~20-24 bytes and ~3 avg cycles per 4-5 case switch that moves from jump table to compare chain.

Repro:
```c
void dispatch(int op, int *p) { switch (op) { case 1: p[0]=1; break; case 2: p[1]=2; break; case 3: p[2]=3; break; case 4: p[3]=4; break; } }
```

Fix: (a) llvm/lib/Target/EZH/EZHConstantIslandPass.cpp:347-365: build LSL_ADD into the (always-killed, freshly materialized) TableReg instead of EZH::RA and drop 'Defs = [RA]' from PseudoBR_JT in EZHInstrInfo.td:574 (or give the pseudo an explicit earlyclobber scratch operand for the allocator). (b) EZHISelLowering constructor: setMinimumJumpTableEntries(6) (measure 6 vs 8; the 7-insn + pool + table fixed overhead and the 2-slot-scheduled ldr-pc argue for the higher value at -Os).

## [7] [DONE cb42ac92] i64 ordered compares and branches expand to a 13-17 insn select chain with 3 callee-saved regs instead of the subs/sbcs carry idiom (SETCCCARRY unimplemented)

Estimated win: ~12-15 insns (48-60 bytes) and 3 callee-saved push/pop pairs saved per i64 ordered compare; branches drop from ~16 insns to 3.

Repro:
```c
int cmplt(unsigned long long a, unsigned long long b) { return a < b; }
void br_lt(unsigned long long a, unsigned long long b) { if (a < b) f(); }
```

Fix: setOperationAction(ISD::SETCCCARRY, MVT::i32, Custom) in EZHISelLowering.cpp; the type legalizer then automatically emits USUBO(lo) + SETCCCARRY(hi) for LT/GE/ULT/UGE and flips GT/LE (LegalizeIntegerTypes.cpp:5917-5951). Lower SETCCCARRY to subs/sbcs + EZHISD::SELECT_CC (or fold into the existing custom BR_CC path) on the CA/NC condition. EZH has carry but no overflow flag, so for the SIGNED LT/GE forms bias both hi words with xor 0x80000000 (load_imm materializes 1<<31 in one insn) and use the unsigned carry -- still ~3 insns extra, far below the current chain. Requires USUBO custom from finding 1 (the legalizer builds the lo half as USUBO).

## [6] Out-of-range constant offsets (word >508, byte >127) pay load_imm + ldr_reg per access instead of rebasing once through a shared anchor; same disease makes every big-frame spill slot cost 2 instructions

Estimated win: 1 instruction (4 bytes + 1 cycle) per access after the first at each anchor; the 3-load repro drops 8->6 body insns; ~300 sites in the corpus's big-frame functions.

Repro:
```c
int far(int *p) { return p[130] + p[131] + p[132]; }
```

Fix: Two layers: (a) EZHISelDAGToDAG.cpp PreprocessISelDAG (X86/AArch64 precedent): rewrite (add base, C) with C out of memory-fold range into (add (add base, C & ~511), C & 511) so the inner anchor CSEs across neighboring accesses and the outer constant folds into ldr/str -- do it pre-selection so plain DAG CSE shares the anchor; (b) for stack slots, implement the virtual-base-register hooks in /Users/foxy/Downloads/llvm-ezh-port/llvm/lib/Target/EZH/EZHRegisterInfo.cpp (requiresVirtualBaseRegisters / needsFrameBaseReg / materializeFrameBaseRegister / resolveFrameIndex) so LocalStackSlotAllocation shares one rebase among out-of-range slots instead of eliminateFrameIndex scavenging per access.

## [6] No predicated bare returns: RET is isPredicable=0, so ~1000 conditional branches jump to a lone 'mov pc, ra' — while the exact same shape with a frame already gets 'popd_ze pc' (silicon-validated), leaf functions never get 'mov_cc pc, ra'

Estimated win: Size-neutral per site (4B goto -> 4B predicated ret) but removes one taken branch (~2 cycles, register-goto is 2-slot scheduled) per dynamic early exit, deletes shared return blocks when all preds convert (-4B), and unblocks the 111 epilogue triangles for the IfConverter; ~1100 sites in the corpus.

Repro:
```c
int sw4(int x) { switch (x) { case 0: return 10; case 1: return 22; case 2: return 35; case 3: return 41; default: return -1; } }   // any early-exit guard produces the same shape
```

Fix: Two parts. (1) llvm/lib/Target/EZH/EZHInstrInfo.td:1063: drop 'let isPredicable = 0' on RET and implement its predication in EZHInstrInfo::PredicateInstruction as MOV(pc, ra, CC) (or a RET_COND pseudo printed as mov_cc pc, ra), letting the IfConverter convert single-pred return triangles including multi-insn epilogues (popd_cc r4; popd_cc pc). (2) A late peephole (fits in the EZHCompareFusion/preEmit slot) rewriting 'goto_cc .L' into 'mov_cc pc, ra' / 'popd_cc pc' when .L consists solely of that return — this covers the dominant shared-return-block case the IfConverter structurally cannot merge. One-time validation of mov_cc-to-PC against the ezhdis oracle/silicon recommended; check interaction with EZHBitSliceInjection which special-cases returns.

## [6] i64 multiply by small constant always calls __muldi3 even though the backend has a shift-add mul combine -- it is gated to i32 only

Estimated win: Replaces a __muldi3 call (5 insns at site + ra save + several-hundred-cycle 64-bit software mul loop) with 4-14 inline insns; ~20-40x cycle win per site, roughly size-neutral at -Os for 1-2-step constants (3, 5, 9, 10, 100, 1000, 216...).

Repro:
```c
unsigned long long mul3(unsigned long long x) { return x * 3; }
unsigned long long us_to_ns(unsigned long long us) { return us * 1000; }
unsigned long long ticks(unsigned t, unsigned long long acc) { return acc + t * 216ULL; }  // all call __muldi3 at -Os and -O2
```

Fix: EZHISelLowering.cpp:1421 -- PerformDAGCombine's mul-by-constant chain builder is `VT == MVT::i32` only; extend to MVT::i64 pre-type-legalization (the combiner runs before types are expanded), emitting i64 SHL/ADD/SUB nodes that then legalize to the 3-insn shift funnel + adds/adc. Also EZHISelLowering.cpp:1445 -- decomposeMulByConstant returns true only for i32, so even 2^n+/-1 i64 constants (x*3) skip the generic decomposition; accept i64 too. Cost model: each i64 chain step is ~5 insns (vs 1 for i32), so budget 2 steps at minsize, 3 at -O2. Land after finding 1, which makes the wide adds 2 insns instead of 9.

## [6] [DONE 0c0627ab] MachineOutliner not ported: heavy exact-sequence duplication, especially expanded 16-bit load/store idioms and CSR pop chains

OUTCOME: ported (whitelist-closed classifier; dedicated OUTLINE_CALL with Defs=[RA] only; RA-dead-and-spilled gate rejects frameless leaves; runs before addPreEmitPass2). Default-on at -Oz ONLY (hasMinSize); available anywhere via -mllvm -enable-machine-outliner. There is NO RA-spill/RegSave fallback -- EZH has a single link register, so a candidate is simply dropped when RA is not already dead across it. MEASURED BENEFIT IS SMALL: the estimate below was a cross-corpus upper bound the outliner cannot reach (it captures only intra-module duplication); actual saving is ~80 bytes on the self-test and exactly 768 bytes across the C tests (12 C executables changed) when forced on at -Os. MEASUREMENT CAVEAT: the forced-outliner corpus run set EXTRA_CFLAGS only via CMAKE_C_FLAGS -- run.sh omitted it from CMAKE_CXX_FLAGS (fixed 2026-07-16) -- so the 30 C++ -Os tests, INCLUDING the EH tests, did NOT exercise forced outlining. The 768-byte figure and the "12 executables changed" are therefore C-only; the forced-mode C++ contribution is unmeasured (a re-run with the fixed script would cover it). Correctness proven three ways: 30/30 host tests, the on-silicon differential (run_outliner.sh: ON vs OFF vs host golden, 1 outlined helper + 24 call sites, byte-identical), and a one-time full -Os corpus run with the outliner forced active (all 3078 tests passed -- correctness holds for C and C++ alike; artifact ezh/llvm_test/ezh_lit_results_outliner_forced.json). Original estimate below is retained for context only.

Estimated win: ~1 insn per site minus (L+1) shared copy per candidate; measured upper bound 42.8KB+2.7KB (12%) on corpus, 3.6KB in the largest firmware demo; biggest single lever but a real feature port

Repro:
```c
Any code using short/uint16_t fields (EZH has only 8/32-bit ld/st, so each i16 access expands to 5-6 insns), e.g. ezh/ezh_test/ezh_comp.c: the 6-insn sequence 'ldrb r1,r1,0 | ldrb r2,r2,0 | lsl r2,r2,24 | lsl_or r1,r2,r1,16 | asr r1,r1,16 | ldrb r2,r3,0' repeats 14x verbatim in one module.
```

Fix (ORIGINAL PLAN -- superseded by OUTCOME above where they differ; the two prescriptions below were deliberately NOT adopted): Implement the TargetInstrInfo outliner hooks in EZHInstrInfo (isFunctionSafeToOutlineFrom, getOutliningTypeImpl, getOutliningCandidateInfo, buildOutlinedFrame, insertOutlinedCall). Call = gosub (1 insn, clobbers RA per td Defs list), return = mov pc, ra. Two plan items changed in the shipped port: (1) default is -Oz ONLY (hasMinSize), NOT -Os/-Oz -- forced-on at -Os was used only for the one-time corpus measurement. (2) There is NO RA-spill fallback (the RISC-V "pushd ra/popd pc in the outlined frame" model does not apply): EZH has a single link register, so a candidate with live (not-already-dead-and-spilled) RA is simply DROPPED rather than spilled. Sequences ending in the popd-chain+return still outline as tail calls (goto).

## [5] Duplicate {load_imm rX, V; return/goto} tails survive branch folding (tail-merge threshold too high for fixed 4-byte insns)

Estimated win: 4-8 bytes per duplicated tail (one-line change); low frequency but zero-risk and it directly retires the known tail-duplication constant-materialization waste

Repro:
```c
gcc-torture pr17133.c main at -O2 (also 20021118-3, 990222-1, pr64260, 20031010-1, pr28651...); any function where several paths end in `return 0;` after calls
```

Fix: Override TargetInstrInfo::getTailMergeSize (hook at llvm/include/llvm/CodeGen/TargetInstrInfo.h:2352, consumed by BranchFolding.cpp:226) in EZHInstrInfo to return 2 (default 3). Every EZH insn is exactly 4 bytes and there is no branch-prediction penalty asymmetry, so MinCommonTailLength=2 is always a size win when >=2 predecessors share the tail.

## [4] i32 divide/modulo by small constants always calls __udivsi3/__umodsi3 (no multiplier, so no magic numbers) -- reciprocal shift-add + correction sequences beat the bit-loop libcall ~10x

Estimated win: ~150-250 cycles -> ~15 cycles per site (~10-15x); size grows ~40 bytes per site so gate at -O2, keep the libcall at -Os/MinSize.

Repro:
```c
unsigned udiv10(unsigned x) { return x / 10; }   // also x/3, x%10 etc., -O2
```

Fix: Custom-lower ISD::UDIV/UREM (and SDIV via sign-fixup) for constant divisors from a small table of shift-add reciprocal recipes in EZHISelLowering.cpp (or a DAGCombine before the LibCall action triggers), gated on OptForSpeed / divisor in table. UREM as x - d*(x/d) reuses the existing mul-by-constant chain (EZHISelLowering.cpp:1421). Lower priority / more work than findings 1-3, but the only path to non-pow2 constant division without a hardware multiply.


## [KNOWN ISSUE] [DONE 725f496] Predicated GOTO carries a static isBarrier, so -verify-machineinstrs fails on any conditional branch

DONE 2026-07-16 (725f496aa321): GOTO split into GOTO (unconditional, isBarrier,
no predicate operand, codegen-only), GOTO_CC (conditional, not a barrier, pred
operand), and GOTOL (conditional link form). analyzeBranch / insertBranch /
PredicateInstruction / EZHBitSliceInjection / EZHConstantIslandPass (incl.
range-tracking GOTO_CC) and the SjLj/custom-inserter branch builders were all
updated. -verify-machineinstrs is now clean at -O0/-O2 and is enabled on every
EZH CodeGen test RUN line. Validated 3078/3078 on silicon at -O0 and -Os
including ezh_eh/ezh_setjmp -- the bare-isBarrier-flip SjLj miscompile did NOT
recur because the split keeps unconditional dispatch branches as barriers. The
predicated-tail-call (TCRETURN) path carries the same unsound shape (a predicated
tail call keeps a fall-through successor while wearing the barrier/return
descriptor); it was split the same way in a follow-up (TCRETURN_CC family) --
byte-identical, since a predicated tail call only arises under
-mno-ezh-bitslice-interrupts in leaf functions.

External review: "MBB exits via conditional branch/fall-through but ends with a
barrier instruction". GOTO is one opcode whose predicate is an operand, but
isBarrier is a static MCID flag: every predicated (conditional) goto can fall
through, contradicting the flag. Semantics are correct today -- analyzeBranch
and friends are predicate-aware -- but the machine verifier cannot run clean,
which blocks using it as a routine safety net.

Fix: split the opcode into an unconditional GOTO (isBarrier = 1, no predicate)
and a conditional GOTO_CC (isBarrier = 0, predicate operand), updating
analyzeBranch / insertBranch / removeBranch / PredicateInstruction /
EZHCompareFusion / EZHBitSliceInjection / EZHConstantIslandPass, plus the
predicated-tail-call selection (TCRETURN carries the same pattern).

Warning from project history: a bare flip of GOTO's isBarrier to 0 was tried in
an earlier session and MISCOMPILED the C++ SjLj exception dispatch (reverted;
generic passes rely on barrier-ness of unconditional branches). The split is
the only sound shape for this fix -- do not retry the flip.

## [KNOWN ISSUE] [DONE b87fb16 for the materializers] Predicated and unpredicated encodings share opcodes and descriptors

DONE 2026-07-16 (b87fb16382a5) for the IMMEDIATE MATERIALIZERS -- the load-bearing
case this item is about (load_imm / load_simm want honest movable descriptors so
they rematerialize). LOAD_IMM/IMMN/SIMM/SIMMN now each have a codegen-only *_CC
twin (hasSideEffects=1, not rematerializable, same encoding + asm), and
PredicateInstruction rewrites the base opcode to its _CC twin instead of just
setting the predicate operand, exactly the fix prescribed below. Byte-identical
codegen (0/244 gcc-torture at -O2, bitslice on and off); 3078/3078 silicon O0+Os.
The isReMaterializableImpl isPredicated guard is now defensive, not load-bearing.
REMAINING (lower value, kept OPEN): the GENERAL predicated ALU ops (add_ze,
sub_nz, mov_cc, ...) still share opcodes and still rely on the no-post-RA-
scheduling pin. Splitting all of them would let targetSchedulesPostRAScheduling
be dropped and a post-RA scheduler run -- but those ops are not remat/CSE
candidates and an in-order core gains little from post-RA scheduling, so the pin
is cheap to keep. Split them only if a concrete need for post-RA scheduling
appears.

Sibling of the GOTO isBarrier issue above, surfaced by external review of the
immediate-remat change: the predicate is an operand, so static MCID flags
cannot distinguish a pure unpredicated load_imm (safe to move, remat, CSE)
from a predicated load_imm_cc (reads unmodelled flags, must not move). The
descriptor dilemma is unsolvable per-instance: LiveRangeEdit gates remat on
MachineInstr::isSafeToMove, which rejects unmodelled side effects with no
target override, so hasSideEffects = 1 kills remat and hasSideEffects = 0
under-describes the predicated instances. Today the pipeline is safe by
construction (predicated instances only exist after the if-converter, and the
post-RA scheduler is pinned off via targetSchedulesPostRAScheduling), but the
complete fix is an opcode split: unpredicated opcodes with honest movable
descriptors, predicated *_CC opcodes with hasSideEffects = 1, and
PredicateInstruction switching opcode instead of rewriting an operand.

STILL OPEN (2026-07-16): this is the remaining half of the predication-modeling
overhaul. The GOTO/GOTO_CC branch split shipped on its own in 725f496 (see the
item above), which is what unblocked -verify-machineinstrs; this general
materializer split (load_imm / load_imm_cc etc.) was NOT part of that commit and
is a modeling-purity item, not a correctness blocker -- the verifier runs clean
today because the pipeline is safe by construction.

## [DEFERRED - unsafe minimal fixes] Jump-table dispatch clobbers RA as scratch, forcing pushd ra in leaf switch functions

The ConstantIslandPass expands PseudoBR_JT to "lsl_add RA, table, index, 2;
ldr pc, RA, 0", and the pseudo declares Defs = [RA], so every leaf function
with a switch saves RA it would not otherwise need. Two tempting minimal fixes
were tried and BOTH regressed on silicon:

 * Reuse the index register as scratch (lsl_add index, table, index, 2). The
   index is dead in the dispatch block, but its physical register can be live
   INTO a case block via another edge (shared/fall-through case code), so the
   in-place write corrupts a live value: shift64 self-test 21/24 wrong. RA was
   used originally precisely because it is reserved and never carries a live
   value across the dispatch.

 * Rewrite dispatch in a vreg at ISel (brind of a folded load) and retire
   PseudoBR_JT. This also required changing getJumpTableIndex and the C++ SjLj
   dispatch block; it regressed both ezh_eh and switch-using tests on silicon.

Safe fix (deferred to its own round): compute the entry address into a GPR
VREG during ISel so the register allocator handles liveness correctly, and
keep PseudoBR_JT carrying the address register + the JTI operand (so
getJumpTableIndex and table emission are untouched); the island expansion
becomes just "ldr pc, addr, 0" with no scratch. The EH DispatchBB must compute
the same vreg address before the pseudo. Validate ezh_eh and a real
jump-table switch on silicon before landing -- this path is adjacent to the
SjLj dispatch that a previous clever change silently miscompiled.
