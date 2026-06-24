//===- EZHSubtarget.cpp - EZH Subtarget Information -----------*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the EZH specific subclass of TargetSubtarget.
//
//===----------------------------------------------------------------------===//

#include "EZHSubtarget.h"

#define DEBUG_TYPE "ezh-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "EZHGenSubtargetInfo.inc"

using namespace llvm;

void EZHSubtarget::initSubtargetFeatures(StringRef CPU, StringRef FS) {
  std::string CPUName = std::string(CPU);
  if (CPUName.empty())
    CPUName = "generic";

  ParseSubtargetFeatures(CPUName, /*TuneCPU*/ CPUName, FS);
}

EZHSubtarget &EZHSubtarget::initializeSubtargetDependencies(StringRef CPU,
                                                                StringRef FS) {
  initSubtargetFeatures(CPU, FS);
  return *this;
}

EZHSubtarget::EZHSubtarget(const Triple &TargetTriple, StringRef Cpu,
                               StringRef FeatureString, const TargetMachine &TM,
                               const TargetOptions & /*Options*/,
                               CodeModel::Model /*CodeModel*/,
                               CodeGenOptLevel /*OptLevel*/)
    : EZHGenSubtargetInfo(TargetTriple, Cpu, /*TuneCPU*/ Cpu, FeatureString),
      InstrInfo(initializeSubtargetDependencies(Cpu, FeatureString)),
      FrameLowering(*this), TLInfo(TM, *this) {}
