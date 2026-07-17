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

def EZHSchedModel : SchedMachineModel {
  // Cycles for a load result to become usable (SRAM access).
  let LoadLatency = 2;

  // Strictly in-order, single-issue core.
  let IssueWidth = 1;
  let MicroOpBufferSize = 0;
  let LoopMicroOpBufferSize = 0;

  // Extra cycles for a mispredicted branch. [default = -1]
  let MispredictPenalty = 10;

  // No per-opcode data; every opcode falls back to default latencies.
  let CompleteModel = 0;

  // The post-RA scheduler is placed explicitly by EZHPassConfig
  // (addPreEmitPass), not via this flag.
  let PostRAScheduler = 0;
}
