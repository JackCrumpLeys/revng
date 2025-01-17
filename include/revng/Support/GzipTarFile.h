#pragma once

//
// This file is distributed under the MIT License. See LICENSE.mit for details.
//

#include "llvm/ADT/StringSet.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include "revng/Support/Assert.h"
#include "revng/Support/Generator.h"

extern "C" {
struct archive;
}

namespace revng {

struct OffsetDescriptor {
  size_t Start;
  size_t DataStart;
  size_t PaddingStart;
  size_t End;

  size_t headerSize() { return DataStart - Start; }
  size_t dataSize() { return PaddingStart - DataStart; }
  size_t paddingSize() { return End - PaddingStart; }
};

/// Class that allows writing a '.tar.gz' file conforming to the PAX archive
/// format. The archive is created with these additional properties:
/// * The header of each file is a stand-alone gzip stream
/// * Each file is packaged in a stand-alone gzip stream
/// * The trailing padding is also a stand-alone gzip stream
///
/// The most important property of this arrangement is that it allows each
/// individual file to be read from the archive without decompressing the entire
/// archive.
///
/// Given the offset and size of a single file, the following command line
/// invocation will retrieve it:
/// \code{.sh}
/// dd if=archive bs=1 skip=$offset count=$size | gunzip
/// \endcode
/// Because of this, the ::append method, which is used to add a file to the
/// archive, returns an OffsetDescriptor object describing the position in the
/// compressed file of where each gzip stream starts and ends. This allows users
/// of this class to make an index file that tracks the location of each file.
///
/// Additionally, since the gzip standard allows concatenating streams, the file
/// produced is still a valid '.tar.gz' file that can be opened by any program
/// that supports "ordinary" '.tar.gz' files.
class GzipTarWriter {
private:
  llvm::raw_ostream *OS = nullptr;
  llvm::StringSet<> Filenames;

public:
  GzipTarWriter(llvm::raw_ostream &OS) : OS(&OS){};
  ~GzipTarWriter() { revng_assert(OS == nullptr); }

  GzipTarWriter(const GzipTarWriter &Other) = delete;
  GzipTarWriter &operator=(const GzipTarWriter &Other) = delete;

  GzipTarWriter(GzipTarWriter &&Other) = default;
  GzipTarWriter &operator=(GzipTarWriter &&Other) = default;

  OffsetDescriptor append(llvm::StringRef Name, llvm::ArrayRef<char> Data);
  void close();
};

struct ArchiveEntry {
  std::string Filename;
  llvm::SmallVector<char> Data;
};

class GzipTarReader {
private:
  archive *Archive = nullptr;

public:
  GzipTarReader(llvm::ArrayRef<char> Ref);
  GzipTarReader(const llvm::MemoryBuffer &Buffer) :
    GzipTarReader({ Buffer.getBufferStart(), Buffer.getBufferSize() }){};
  ~GzipTarReader();

  GzipTarReader(const GzipTarReader &Other) = delete;
  GzipTarReader &operator=(const GzipTarReader &Other) = delete;

  GzipTarReader(GzipTarReader &&Other) = default;
  GzipTarReader &operator=(GzipTarReader &&Other) = default;

  cppcoro::generator<ArchiveEntry> entries();
};

} // namespace revng
