/// \file GzipTarFile.cpp

//
// This file is distributed under the MIT License. See LICENSE.mit for details.
//

#include "revng/Support/GzipStream.h"
#include "revng/Support/GzipTarFile.h"

#define BOOST_TEST_MODULE GzipTarFile
bool init_unit_test();
#include "boost/test/unit_test.hpp"

#include "revng/UnitTestHelpers/UnitTestHelpers.h"

static std::string gzipDecompress(llvm::ArrayRef<char> Buffer) {
  llvm::SmallString<128> Output;
  llvm::raw_svector_ostream OS(Output);
  gzipDecompress(OS, Buffer);
  return Output.str().str();
}

static void checkOffset(llvm::SmallVector<char> &Buffer,
                        size_t Start,
                        size_t Size,
                        llvm::StringRef ExpectedValue) {
  std::string Result = gzipDecompress({ Buffer.data() + Start, Size });
  BOOST_TEST(Result == ExpectedValue.str());
}

BOOST_AUTO_TEST_CASE(GzipTarFileTest) {
  using revng::ArchiveEntry;
  using revng::OffsetDescriptor;

  llvm::SmallVector<char> Buffer;
  llvm::raw_svector_ostream OS(Buffer);

  // Write test data
  revng::GzipTarWriter Writer(OS);

  const char Data1[5] = "foo2";
  OffsetDescriptor Offset1 = Writer.append("foo", { Data1, 4 });

  const char Data2[5] = "bar2";
  OffsetDescriptor Offset2 = Writer.append("bar", { Data2, 4 });

  Writer.close();

  BOOST_TEST(Offset1.Start == 0ULL);
  BOOST_TEST(Offset2.Start == Offset1.End);

  {
    revng::GzipTarReader Reader({ Buffer.data(), Buffer.size() });

    cppcoro::generator<ArchiveEntry> Gen = Reader.entries();
    std::vector<ArchiveEntry> Entries(Gen.begin(), Gen.end());
    BOOST_TEST(Entries.size() == 2ULL);

    llvm::StringRef RefData1(Entries[0].Data.data(), Entries[0].Data.size());
    BOOST_TEST(Entries[0].Filename == "foo");
    BOOST_TEST(RefData1.str() == "foo2");

    llvm::StringRef RefData2(Entries[1].Data.data(), Entries[1].Data.size());
    BOOST_TEST(Entries[1].Filename == "bar");
    BOOST_TEST(RefData2.str() == "bar2");
  }

  checkOffset(Buffer, Offset1.DataStart, Offset1.dataSize(), "foo2");
  checkOffset(Buffer, Offset2.DataStart, Offset2.dataSize(), "bar2");
}
