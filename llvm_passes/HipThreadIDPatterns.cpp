//===- HipThreadIDPatterns.cpp --------------------------------------------===//
//
// Part of the chipStar Project, under the Apache License v2.0 with LLVM
// Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Folds threadIdx, BlockIdx and blockDim patterns like:
//
//  * blockIdx.x * blockDim.x + threadIdx.x --> get_global_id(0)
//
// (c) 2024 Henry Linjamäki / Intel
//===----------------------------------------------------------------------===//
#include "HipThreadIDPatterns.h"

#include "LLVMSPIRV.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/IR/PatternMatch.h>

#define PASS_NAME "hip-tid-patterns"
#define DEBUG_TYPE PASS_NAME

using namespace llvm;

static bool isaFnCall(Value *V, StringRef Name) {
  auto *CI = dyn_cast<CallInst>(V);
  if (!CI)
    return false;

  auto *Callee = CI->getCalledFunction();

  if (!Callee || !Callee->hasName())
    return false;

  return Callee->getName() == Name;
}

static Value* getCallOperand(Value *V, unsigned OpIdx) {
  auto *CI = cast<CallInst>(V);
  return CI->getOperand(OpIdx);
}

static Value* isGetGlobalIDPattern(Value *V) {
  constexpr auto GetGroupId = "_Z12get_group_idj";
  constexpr auto GetLocalSize = "_Z14get_local_sizej";
  constexpr auto GetLocalId = "_Z12get_local_idj";

  Value *Mul0, *Mul1, *Add0;
  using namespace llvm::PatternMatch;
  auto m_Muladd = m_Add(m_OneUse(m_Mul(m_TruncOrSelf(m_Value(Mul0)),
                                       m_TruncOrSelf(m_Value(Mul1)))),
                        m_TruncOrSelf(m_Value(Add0)));

  if (match(V, m_Muladd)) {
    if (((isaFnCall(Mul0, GetGroupId) && isaFnCall(Mul1, GetLocalSize)) ||
         (isaFnCall(Mul1, GetGroupId) && isaFnCall(Mul0, GetLocalSize))) &&
        isaFnCall(Add0, GetLocalId)) {

      // Check the dimension arguments are the same value.
      auto *Arg0 = getCallOperand(Mul0, 0);
      auto *Arg1 = getCallOperand(Mul1, 0);
      auto *Arg2 = getCallOperand(Add0, 0);
      if (Arg0 != Arg1 || Arg1 != Arg2)
        return nullptr;

      return Arg0;
    }
  }

  return nullptr;
}

static Value *createTruncOrSelf(Value *V, Type *Ty,
                                Instruction *InsertBefore = nullptr) {
  if (V->getType() == Ty)
    return V;
  return CastInst::CreateTruncOrBitCast(V, Ty, "", InsertBefore);
}

PreservedAnalyses HipThreadIDPatternsPass::run(Module &M,
                                             ModuleAnalysisManager &AM) {

  constexpr auto GetGlobalId = "_Z13get_global_idj";
  auto *I32Ty = IntegerType::get(M.getContext(), 32);
  auto *I64Ty = IntegerType::get(M.getContext(), 64);

  bool Changed = false;
    for (auto &F : M)
      for (auto &BB : F)
        for (auto &I : BB)
          if (Value *Dim = isGetGlobalIDPattern(&I)) {
            FunctionCallee GIDFn =
                M.getOrInsertFunction(GetGlobalId, I64Ty, I32Ty);
            auto *GIDCall = CallInst::Create(GIDFn, {Dim}, "gid", &I);
            auto *CastedGID = createTruncOrSelf(GIDCall, I.getType(), &I);
            I.replaceAllUsesWith(CastedGID);
            Changed = true;
          }

  return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, PASS_NAME, LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == PASS_NAME) {
                    MPM.addPass(HipThreadIDPatternsPass());
                    return true;
                  }
                  return false;
                });
          }};
}
