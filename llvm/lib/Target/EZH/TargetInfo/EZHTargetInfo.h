//===-- EZHTargetInfo.h - EZH Target Implementation ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Description:
//   Declares global accessor getTheEZHTarget(), returning the singleton Target
//   reference for EZH.
//
// Copied From:
//   Lanai target backend (llvm/lib/Target/Lanai/TargetInfo/LanaiTargetInfo.h).
//
// Changes:
//   Renamed getTheLanaiTarget to getTheEZHTarget.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EZH_TARGETINFO_EZHTARGETINFO_H
#define LLVM_LIB_TARGET_EZH_TARGETINFO_EZHTARGETINFO_H

namespace llvm {

class Target;

Target &getTheEZHTarget();

} // namespace llvm

#endif // LLVM_LIB_TARGET_EZH_TARGETINFO_EZHTARGETINFO_H
