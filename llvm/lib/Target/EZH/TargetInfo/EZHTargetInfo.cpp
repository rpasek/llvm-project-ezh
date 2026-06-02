//===-- EZHTargetInfo.cpp - EZH Target Implementation -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Description:
//   Implements TargetRegistry::RegisterTarget entry point for registering the
//   EZH target architecture (ezh).
//
// Copied From:
//   Lanai target backend
//   (llvm/lib/Target/Lanai/TargetInfo/LanaiTargetInfo.cpp).
//
// Changes:
//   Registered the ezh target name and description (NXP EZH / SmartDMA Core).
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/EZHTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

Target &llvm::getTheEZHTarget() {
  static Target TheEZHTarget;
  return TheEZHTarget;
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeEZHTargetInfo() {
  RegisterTarget<Triple::ezh> X(getTheEZHTarget(), "ezh", "EZH", "EZH");
}
