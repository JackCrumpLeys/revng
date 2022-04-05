#pragma once

//
// This file is distributed under the MIT License. See LICENSE.md for details.
//

#include "llvm/IR/Instructions.h"

#include "revng/Model/Binary.h"
#include "revng/Support/Assert.h"
#include "revng/Support/IRHelpers.h"
#include "revng/Support/MetaAddress.h"

inline MetaAddress getMetaAddressOfIsolatedFunction(const llvm::Function &F) {
  revng_assert(FunctionTags::Isolated.isTagOf(&F));
  return getMetaAddressMetadata(&F, FunctionEntryMDNName);
}

inline model::Function *
llvmToModelFunction(model::Binary &Binary, const llvm::Function &F) {
  auto MaybeMetaAddress = getMetaAddressMetadata(&F, FunctionEntryMDNName);
  if (MaybeMetaAddress == MetaAddress::invalid())
    return nullptr;
  if (auto It = Binary.Functions.find(MaybeMetaAddress);
      It != Binary.Functions.end())
    return &(*It);

  return nullptr;
}

inline const model::Function *
llvmToModelFunction(const model::Binary &Binary, const llvm::Function &F) {
  auto MaybeMetaAddress = getMetaAddressMetadata(&F, FunctionEntryMDNName);
  if (MaybeMetaAddress == MetaAddress::invalid())
    return nullptr;
  if (auto It = Binary.Functions.find(MaybeMetaAddress);
      It != Binary.Functions.end())
    return &*It;

  return nullptr;
}