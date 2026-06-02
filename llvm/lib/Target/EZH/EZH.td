//===- EZH.td - Describe the EZH Target Machine --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Description:
//   Top-level TableGen target definition file defining processor models,
//   subtarget features, and importing core register, instruction, and calling
//   convention definitions.
//
// Copied From:
//   Lanai target backend (llvm/lib/Target/Lanai/Lanai.td).
//
// Changes:
//   Updated includes to EZHRegisterInfo.td, EZHInstrInfo.td, and
//   EZHCallingConv.td; defined EZH target machine properties and
//   subtarget features.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Target-independent interfaces which we are implementing
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// Register File, Calling Conv, Instruction Descriptions
//===----------------------------------------------------------------------===//

include "EZHRegisterInfo.td"
include "EZHCallingConv.td"
include "EZHInstrInfo.td"

defm : RemapAllTargetPseudoPointerOperands<GPR>;

def EZHInstrInfo : InstrInfo;

//===----------------------------------------------------------------------===//
// EZH Subtarget features.
//===----------------------------------------------------------------------===//

def FeatureBitSliceInterrupts : SubtargetFeature<
    "bitslice-interrupts", "HasBitSliceInterrupts", "true",
    "Enable EZH bitslice interrupt workaround (injection of gotol_bs "
    "before branches/calls)">;

//===----------------------------------------------------------------------===//
// EZH processors supported.
//===----------------------------------------------------------------------===//

def : ProcessorModel<"generic", NoSchedModel, [FeatureBitSliceInterrupts]>;
def : ProcessorModel<"v11", NoSchedModel, [FeatureBitSliceInterrupts]>;

def EZHInstPrinter : AsmWriter {
  string AsmWriterClassName  = "InstPrinter";
  bit isMCAsmWriter = 1;
}

//===----------------------------------------------------------------------===//
// Declare the target which we are implementing
//===----------------------------------------------------------------------===//

def EZH : Target {
  // Pull in Instruction Info:
  let InstructionSet = EZHInstrInfo;
  let AssemblyWriters = [EZHInstPrinter];
}
