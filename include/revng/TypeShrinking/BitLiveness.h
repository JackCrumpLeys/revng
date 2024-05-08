#pragma once

//
// This file is distributed under the MIT License. See LICENSE.mit for details.
//

#include <map>
#include <optional>

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

namespace TypeShrinking {

extern const uint32_t Top;

bool isDataFlowSink(const llvm::Instruction *Ins);

struct InstructionResults {
  unsigned Operands = 0;
  unsigned Result = 0;
};

using BitLivenessAnalysisResults = std::map<llvm::Instruction *,
                                            InstructionResults>;

class BitLivenessWrapperPass : public llvm::FunctionPass {
public:
  static char ID; // Pass identification, replacement for typeid
  BitLivenessWrapperPass() : FunctionPass(ID) {}

  bool runOnFunction(llvm::Function &F) override;

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }

  BitLivenessAnalysisResults &getResult() { return Result; }

private:
  BitLivenessAnalysisResults Result;
};

class BitLivenessPass : public llvm::AnalysisInfoMixin<BitLivenessPass> {
  friend llvm::AnalysisInfoMixin<BitLivenessPass>;

private:
  static llvm::AnalysisKey Key;

public:
  using Result = BitLivenessAnalysisResults;

public:
  Result run(llvm::Function &F, llvm::FunctionAnalysisManager &);
};

} // namespace TypeShrinking
