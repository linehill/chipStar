//===- HipThreadIDPatterns.h ----------------------------------------------===//
//
// Part of the chipStar Project, under the Apache License v2.0 with LLVM
// Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// (c) 2024 Henry Linjamäki / Intel
//===----------------------------------------------------------------------===//

#ifndef LLVM_PASSES_HIP_THREAD_ID_PATTERNS_H
#define LLVM_PASSES_HIP_THREAD_ID_PATTERNS_H

#include <llvm/IR/PassManager.h>

using namespace llvm;

class HipThreadIDPatternsPass : public PassInfoMixin<HipThreadIDPatternsPass> {
public:
  PreservedAnalyses run(Module &F, ModuleAnalysisManager &AM);
  static bool isRequired() { return false; }
};

#endif // LLVM_PASSES_HIP_THREAD_ID_PATTERNS_H
