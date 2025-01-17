/// \file Path.cpp

//
// This file is distributed under the MIT License. See LICENSE.mit for details.
//

#include "revng/Storage/Path.h"
#include "revng/Storage/StorageClient.h"

#include "LocalStorageClient.h"
#include "StdStorageClient.h"

namespace {

// TODO: unix-specific, will not work on Windows
revng::LocalStorageClient LocalClient("/");
revng::StdinStorageClient StdinClient;
revng::StdoutStorageClient StdoutClient;

std::string normalizeLocalPath(llvm::StringRef Path) {
  llvm::SmallString<256> PathCopy(Path);
  std::error_code MakeAbsoluteError = llvm::sys::fs::make_absolute(PathCopy);
  revng_assert(not MakeAbsoluteError);
  llvm::sys::path::remove_dots(PathCopy, true);
  llvm::StringRef Result(PathCopy.substr(1)); // Remove leading '/'
  return Result.str();
}

} // namespace

namespace revng {

DirectoryPath DirectoryPath::fromLocalStorage(llvm::StringRef Path) {
  return revng::DirectoryPath{ &LocalClient, normalizeLocalPath(Path) };
}

FilePath FilePath::stdin() {
  return revng::FilePath{ &StdinClient, "" };
}

FilePath FilePath::stdout() {
  return revng::FilePath{ &StdoutClient, "" };
}

FilePath FilePath::fromLocalStorage(llvm::StringRef Path) {
  return revng::FilePath{ &LocalClient, normalizeLocalPath(Path) };
}

} // namespace revng
