/// \file LinkForTranslation.cpp
/// The link for translation pipe is used to link object files into a
/// executable.

//
// This file is distributed under the MIT License. See LICENSE.mit for details.
//

#include "revng/Pipeline/AllRegistries.h"
#include "revng/Pipes/ModelGlobal.h"
#include "revng/Recompile/LinkForTranslation.h"
#include "revng/Recompile/LinkForTranslationPipe.h"
#include "revng/Support/ResourceFinder.h"

using namespace llvm;
using namespace llvm::sys;
using namespace pipeline;
using namespace ::revng::pipes;

void LinkForTranslation::run(const ExecutionContext &Ctx,
                             BinaryFileContainer &InputBinary,
                             ObjectFileContainer &ObjectFile,
                             TranslatedFileContainer &OutputBinary) {
  if (not InputBinary.exists() or not ObjectFile.exists())
    return;

  const model::Binary &Model = *getModelFromContext(Ctx);
  linkForTranslation(Model,
                     *InputBinary.path(),
                     *ObjectFile.path(),
                     OutputBinary.getOrCreatePath());
}

void LinkForTranslation::print(const Context &Ctx,
                               llvm::raw_ostream &OS,
                               llvm::ArrayRef<std::string> Names) const {
  OS << "(unavailable)\n";
}

static RegisterPipe<LinkForTranslation> E5;
