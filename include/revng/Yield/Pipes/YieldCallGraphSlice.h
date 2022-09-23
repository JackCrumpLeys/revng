#pragma once

//
// This file is distributed under the MIT License. See LICENSE.md for details.
//

#include <array>
#include <string>

#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/raw_ostream.h"

#include "revng/Pipeline/Contract.h"
#include "revng/Pipes/FunctionStringMap.h"

namespace revng::pipes {

class YieldCallGraphSlice {
public:
  static constexpr const auto Name = "YieldCallGraphSlice";

public:
  inline std::array<pipeline::ContractGroup, 1> getContract() const {
    pipeline::Contract BinaryContract(kinds::BinaryCrossRelations,
                                      pipeline::Exactness::Exact,
                                      1,
                                      pipeline::InputPreservation::Preserve);

    pipeline::Contract FunctionContract(kinds::Isolated,
                                        pipeline::Exactness::Exact,
                                        0,
                                        kinds::CallGraphSliceSVG,
                                        2,
                                        pipeline::InputPreservation::Preserve);

    return { pipeline::ContractGroup{ std::move(BinaryContract),
                                      std::move(FunctionContract) } };
  }

public:
  void run(pipeline::Context &Context,
           const pipeline::LLVMContainer &TargetList,
           const FileContainer &InputFile,
           FunctionStringMap &Output);

  void print(const pipeline::Context &Ctx,
             llvm::raw_ostream &OS,
             llvm::ArrayRef<std::string> ContainerNames) const;
};

} // namespace revng::pipes