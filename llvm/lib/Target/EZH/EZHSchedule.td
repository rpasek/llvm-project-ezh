//=-EZHSchedule.td - EZH Scheduling Definitions --*- tablegen -*-=========//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A deliberately COARSE machine model: per-instruction properties only, no
// itineraries and no per-opcode SchedWrite bindings. Both omissions are
// load-bearing:
//
//  - Itineraries would flip the ISel-time SelectionDAG scheduler (default
//    ILP preference) into latency-chasing mode, hoisting loads pre-RA,
//    extending live ranges, and growing spill code on this 8-GPR machine
//    (measured +0.25% corpus size when present).
//
//  - Unbound WriteRes tables make TargetSchedModel::hasInstrSchedModel()
//    true, so computeOperandLatency resolves every unbound instruction to
//    an empty scheduling class INSTEAD of falling back to
//    TargetInstrInfo::defaultDefLatency -- which silently turns the
//    LoadLatency below into 1 and defeats the post-RA scheduler's whole
//    purpose.
//
// With neither, the post-RA list scheduler gets exactly the intended coarse
// timing: mayLoad instructions take LoadLatency, everything else 1 cycle.
// Pre-RA scheduling stays register-pressure-focused (see
// EZHSubtarget::overrideSchedPolicy).
//
//===----------------------------------------------------------------------===//

// Per-def latency tuning for the post/pre-indexed loads: their FIRST def
// (the loaded data) takes the AN14650 E_LDR latency of >= 2 cycles. The
// SECOND def (the pointer writeback) is modeled at 1 cycle -- an INFERENCE,
// not a documented or silicon-measured fact: AN14650 gives the data timing
// and the consumer interlock but never states when the writeback is
// available; a post-increment is address arithmetic, so the ALU-path
// assumption is the natural reading. Either way the choice is
// correctness-neutral: the documented hardware interlock stalls any early
// consumer, so scheduling latencies here only shape performance. The
// blanket defaultDefLatency fallback would give BOTH
// defs the 2-cycle load latency, inserting phantom stalls on the pointer
// recurrence that is the critical path of tight_loop pump bodies
// (ldr_post/str_post chains). Everything else stays UNBOUND on purpose:
// TargetSchedModel::computeOperandLatency falls back to defaultDefLatency
// (mayLoad -> LoadLatency, else 1) for opcodes whose sched class carries no
// write-latency entries, which encodes the documented timing already
// (E_STR/E_PER_*/E_PUSH-POP: "performance characteristics same as E_LDR";
// stores produce only the 1-cycle writeback def). Keep the binding surface
// minimal -- an earlier draft's UNBOUND WriteRes tables silently disabled
// the LoadLatency fallback through a different consumer path; re-verify the
// load-hoisting probe after any change here.
def EZHWriteLoad : SchedWrite; // AHB read data: >= 2 cycles (AN14650 E_LDR)
def EZHWriteWB : SchedWrite;   // pointer writeback: ALU path, 1 cycle

def EZHSchedModel : SchedMachineModel {
  // Cycles for a load result to become usable (SRAM access).
  //
  // Silicon note (ezh/TIGHT_LOOP_TIMING.md): a load really occupies the
  // core for 2 issue slots with data usable 3 cycles after issue start --
  // but 2 is still the right value HERE by exact cancellation: the
  // scheduler models the load as single-slot issue, so "latency 2" makes
  // it prefer one filler instruction between load and use, and one filler
  // is precisely what the hardware absorbs for free. A "truer" 3 would
  // chase a second filler for zero measured gain.
  let LoadLatency = 2;

  // Strictly in-order, single-issue core.
  let IssueWidth = 1;
  let MicroOpBufferSize = 0;
  let LoopMicroOpBufferSize = 0;

  // Extra cycles for a mispredicted branch. [default = -1]
  let MispredictPenalty = 10;

  // Almost no per-opcode data: everything except the indexed loads (bound
  // via InstRW below) falls back to default latencies.
  let CompleteModel = 0;

  // The post-RA scheduler is placed explicitly by EZHPassConfig
  // (addPreEmitPass), not via this flag.
  let PostRAScheduler = 0;
}

let SchedModel = EZHSchedModel in {
  def : WriteRes<EZHWriteLoad, []> { let Latency = 2; }
  def : WriteRes<EZHWriteWB, []> { let Latency = 1; }
  // Indexed loads: defs are (data, pointer-writeback) in that order.
  def : InstRW<[EZHWriteLoad, EZHWriteWB],
               (instregex "^LDR(B|BS)?_(PRE|POST)$")>;
}
