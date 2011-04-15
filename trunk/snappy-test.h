// Copyright 2011 Google Inc. All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Various stubs for the unit tests for the open-source version of Snappy.

#ifndef UTIL_SNAPPY_OPENSOURCE_SNAPPY_TEST_H_
#define UTIL_SNAPPY_OPENSOURCE_SNAPPY_TEST_H_

#include "snappy-stubs-internal.h"

#include <stdio.h>
#include <stdarg.h>

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#include <sys/time.h>

#include <string>

#ifdef HAVE_GTEST

#include <gtest/gtest.h>
#undef TYPED_TEST
#define TYPED_TEST TEST
#define INIT_GTEST(argc, argv) ::testing::InitGoogleTest(argc, *argv)

#else

// Stubs for if the user doesn't have Google Test installed.

#define TEST(test_case, test_subcase) \
  void Test_ ## test_case ## _ ## test_subcase()
#define INIT_GTEST(argc, argv)

#define TYPED_TEST TEST
#define EXPECT_EQ CHECK_EQ
#define EXPECT_NE CHECK_NE
#define EXPECT_FALSE(cond) CHECK(!(cond))

#endif

#ifdef HAVE_GFLAGS

#include <gflags/gflags.h>

// This is tricky; both gflags and Google Test want to look at the command line
// arguments. Google Test seems to be the most happy with unknown arguments,
// though, so we call it first and hope for the best.
#define InitGoogle(argv0, argc, argv, remove_flags) \
  INIT_GTEST(argc, argv); \
  google::ParseCommandLineFlags(argc, argv, remove_flags);

#else

// If we don't have the gflags package installed, these can only be
// changed at compile time.
#define DEFINE_int32(flag_name, default_value, description) \
  static int FLAGS_ ## flag_name = default_value;

#define InitGoogle(argv0, argc, argv, remove_flags) \
  INIT_GTEST(argc, argv)

#endif

#ifdef HAVE_LIBZ
#include "zlib.h"
#endif

#ifdef HAVE_LIBLZO2
#include "lzo/lzo1x.h"
#endif

#ifdef HAVE_LIBLZF
extern "C" {
#include "lzf.h"
}
#endif

#ifdef HAVE_LIBFASTLZ
#include "fastlz.h"
#endif

#ifdef HAVE_LIBQUICKLZ
#include "quicklz.h"
#endif

namespace {
namespace File {
  void Init() { }

  void ReadFileToStringOrDie(const char* filename, string* data) {
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {
      perror(filename);
      exit(1);
    }

    data->clear();
    while (!feof(fp)) {
      char buf[4096];
      size_t ret = fread(buf, 1, 4096, fp);
      if (ret == -1) {
        perror("fread");
        exit(1);
      }
      data->append(string(buf, ret));
    }

    fclose(fp);
  }

  void ReadFileToStringOrDie(const string& filename, string* data) {
    ReadFileToStringOrDie(filename.c_str(), data);
  }

  void WriteStringToFileOrDie(const string& str, const char* filename) {
    FILE* fp = fopen(filename, "wb");
    if (fp == NULL) {
      perror(filename);
      exit(1);
    }

    int ret = fwrite(str.data(), str.size(), 1, fp);
    if (ret != 1) {
      perror("fwrite");
      exit(1);
    }

    fclose(fp);
  }
}  // namespace File
}  // namespace

namespace snappy {

#define FLAGS_test_random_seed 301
typedef string TypeParam;

void Test_CorruptedTest_VerifyCorrupted();
void Test_Snappy_SimpleTests();
void Test_Snappy_MaxBlowup();
void Test_Snappy_RandomData();
void Test_Snappy_FourByteOffset();
void Test_SnappyCorruption_TruncatedVarint();
void Test_SnappyCorruption_UnterminatedVarint();
void Test_Snappy_ReadPastEndOfBuffer();
void Test_Snappy_FindMatchLength();
void Test_Snappy_FindMatchLengthRandom();

string ReadTestDataFile(const string& base);

// A sprintf() variant that returns a std::string.
// Not safe for general use due to truncation issues.
string StringPrintf(const char* format, ...);

// A simple, non-cryptographically-secure random generator.
class ACMRandom {
 public:
  explicit ACMRandom(uint32 seed) : seed_(seed) {}

  int32 Next();

  int32 Uniform(int32 n) {
    return Next() % n;
  }
  uint8 Rand8() {
    return static_cast<uint8>((Next() >> 1) & 0x000000ff);
  }
  bool OneIn(int X) { return Uniform(X) == 0; }

  // Skewed: pick "base" uniformly from range [0,max_log] and then
  // return "base" random bits.  The effect is to pick a number in the
  // range [0,2^max_log-1] with bias towards smaller numbers.
  int32 Skewed(int max_log);

 private:
  static const uint32 M = 2147483647L;   // 2^31-1
  uint32 seed_;
};

inline int32 ACMRandom::Next() {
  static const uint64 A = 16807;  // bits 14, 8, 7, 5, 2, 1, 0
  // We are computing
  //       seed_ = (seed_ * A) % M,    where M = 2^31-1
  //
  // seed_ must not be zero or M, or else all subsequent computed values
  // will be zero or M respectively.  For all other values, seed_ will end
  // up cycling through every number in [1,M-1]
  uint64 product = seed_ * A;

  // Compute (product % M) using the fact that ((x << 31) % M) == x.
  seed_ = (product >> 31) + (product & M);
  // The first reduction may overflow by 1 bit, so we may need to repeat.
  // mod == M is not possible; using > allows the faster sign-bit-based test.
  if (seed_ > M) {
    seed_ -= M;
  }
  return seed_;
}

inline int32 ACMRandom::Skewed(int max_log) {
  const int32 base = (Next() - 1) % (max_log+1);
  return (Next() - 1) & ((1u << base)-1);
}

// A wall-time clock. This stub is not super-accurate, nor resistant to the
// system time changing.
class CycleTimer {
 public:
  CycleTimer() : real_time_us_(0) {}

  void Start() {
    gettimeofday(&start_, NULL);
  }

  void Stop() {
    struct timeval stop;
    gettimeofday(&stop, NULL);

    real_time_us_ += 1000000 * (stop.tv_sec - start_.tv_sec);
    real_time_us_ += (stop.tv_usec - start_.tv_usec);
  }

  double Get() {
    return real_time_us_ * 1e-6;
  }

 private:
  int64 real_time_us_;
  struct timeval start_;
};

// Minimalistic microbenchmark framework.

typedef void (*BenchmarkFunction)(int, int);

class Benchmark {
 public:
  Benchmark(const string& name, BenchmarkFunction function) :
      name_(name), function_(function) {}

  Benchmark* DenseRange(int start, int stop) {
    start_ = start;
    stop_ = stop;
    return this;
  }

  void Run();

 private:
  const string name_;
  const BenchmarkFunction function_;
  int start_, stop_;
};
#define BENCHMARK(benchmark_name) \
  Benchmark* Benchmark_ ## benchmark_name = \
          (new Benchmark(#benchmark_name, benchmark_name))

extern Benchmark* Benchmark_BM_UFlat;
extern Benchmark* Benchmark_BM_UValidate;
extern Benchmark* Benchmark_BM_ZFlat;

void ResetBenchmarkTiming();
void StartBenchmarkTiming();
void StopBenchmarkTiming();
void SetBenchmarkLabel(const string& str);
void SetBenchmarkBytesProcessed(int64 bytes);

#ifdef HAVE_LIBZ

// Object-oriented wrapper around zlib.
class ZLib {
 public:
  ZLib();
  ~ZLib();

  // Wipe a ZLib object to a virgin state.  This differs from Reset()
  // in that it also breaks any state.
  void Reinit();

  // Call this to make a zlib buffer as good as new.  Here's the only
  // case where they differ:
  //    CompressChunk(a); CompressChunk(b); CompressChunkDone();   vs
  //    CompressChunk(a); Reset(); CompressChunk(b); CompressChunkDone();
  // You'll want to use Reset(), then, when you interrupt a compress
  // (or uncompress) in the middle of a chunk and want to start over.
  void Reset();

  // According to the zlib manual, when you Compress, the destination
  // buffer must have size at least src + .1%*src + 12.  This function
  // helps you calculate that.  Augment this to account for a potential
  // gzip header and footer, plus a few bytes of slack.
  static int MinCompressbufSize(int uncompress_size) {
    return uncompress_size + uncompress_size/1000 + 40;
  }

  // Compresses the source buffer into the destination buffer.
  // sourceLen is the byte length of the source buffer.
  // Upon entry, destLen is the total size of the destination buffer,
  // which must be of size at least MinCompressbufSize(sourceLen).
  // Upon exit, destLen is the actual size of the compressed buffer.
  //
  // This function can be used to compress a whole file at once if the
  // input file is mmap'ed.
  //
  // Returns Z_OK if success, Z_MEM_ERROR if there was not
  // enough memory, Z_BUF_ERROR if there was not enough room in the
  // output buffer. Note that if the output buffer is exactly the same
  // size as the compressed result, we still return Z_BUF_ERROR.
  // (check CL#1936076)
  int Compress(Bytef *dest, uLongf *destLen,
               const Bytef *source, uLong sourceLen);

  // Uncompresses the source buffer into the destination buffer.
  // The destination buffer must be long enough to hold the entire
  // decompressed contents.
  //
  // Returns Z_OK on success, otherwise, it returns a zlib error code.
  int Uncompress(Bytef *dest, uLongf *destLen,
                 const Bytef *source, uLong sourceLen);

  // Uncompress data one chunk at a time -- ie you can call this
  // more than once.  To get this to work you need to call per-chunk
  // and "done" routines.
  //
  // Returns Z_OK if success, Z_MEM_ERROR if there was not
  // enough memory, Z_BUF_ERROR if there was not enough room in the
  // output buffer.

  int UncompressAtMost(Bytef *dest, uLongf *destLen,
                       const Bytef *source, uLong *sourceLen);

  // Checks gzip footer information, as needed.  Mostly this just
  // makes sure the checksums match.  Whenever you call this, it
  // will assume the last 8 bytes from the previous UncompressChunk
  // call are the footer.  Returns true iff everything looks ok.
  bool UncompressChunkDone();

 private:
  int InflateInit();       // sets up the zlib inflate structure
  int DeflateInit();       // sets up the zlib deflate structure

  // These init the zlib data structures for compressing/uncompressing
  int CompressInit(Bytef *dest, uLongf *destLen,
                   const Bytef *source, uLong *sourceLen);
  int UncompressInit(Bytef *dest, uLongf *destLen,
                     const Bytef *source, uLong *sourceLen);
  // Initialization method to be called if we hit an error while
  // uncompressing. On hitting an error, call this method before
  // returning the error.
  void UncompressErrorInit();

  // Helper function for Compress
  int CompressChunkOrAll(Bytef *dest, uLongf *destLen,
                         const Bytef *source, uLong sourceLen,
                         int flush_mode);
  int CompressAtMostOrAll(Bytef *dest, uLongf *destLen,
                          const Bytef *source, uLong *sourceLen,
                          int flush_mode);

  // Likewise for UncompressAndUncompressChunk
  int UncompressChunkOrAll(Bytef *dest, uLongf *destLen,
                           const Bytef *source, uLong sourceLen,
                           int flush_mode);

  int UncompressAtMostOrAll(Bytef *dest, uLongf *destLen,
                            const Bytef *source, uLong *sourceLen,
                            int flush_mode);

  // Initialization method to be called if we hit an error while
  // compressing. On hitting an error, call this method before
  // returning the error.
  void CompressErrorInit();

  int compression_level_;   // compression level
  int window_bits_;         // log base 2 of the window size used in compression
  int mem_level_;           // specifies the amount of memory to be used by
                            // compressor (1-9)
  z_stream comp_stream_;    // Zlib stream data structure
  bool comp_init_;          // True if we have initialized comp_stream_
  z_stream uncomp_stream_;  // Zlib stream data structure
  bool uncomp_init_;        // True if we have initialized uncomp_stream_

  // These are used only with chunked compression.
  bool first_chunk_;       // true if we need to emit headers with this chunk
};

#endif  // HAVE_LIBZ

}  // namespace snappy

DECLARE_bool(run_microbenchmarks);

static void RunSpecifiedBenchmarks() {
  if (!FLAGS_run_microbenchmarks) {
    return;
  }

  fprintf(stderr, "Running microbenchmarks.\n");
#ifndef NDEBUG
  fprintf(stderr, "WARNING: Compiled with assertions enabled, will be slow.\n");
#endif
#ifndef __OPTIMIZE__
  fprintf(stderr, "WARNING: Compiled without optimization, will be slow.\n");
#endif
  fprintf(stderr, "Benchmark            Time(ns)    CPU(ns) Iterations\n");
  fprintf(stderr, "---------------------------------------------------\n");

  snappy::Benchmark_BM_UFlat->Run();
  snappy::Benchmark_BM_UValidate->Run();
  snappy::Benchmark_BM_ZFlat->Run();

  fprintf(stderr, "\n");
}

#ifndef HAVE_GTEST

static inline int RUN_ALL_TESTS() {
  fprintf(stderr, "Running correctness tests.\n");
  snappy::Test_CorruptedTest_VerifyCorrupted();
  snappy::Test_Snappy_SimpleTests();
  snappy::Test_Snappy_MaxBlowup();
  snappy::Test_Snappy_RandomData();
  snappy::Test_Snappy_FourByteOffset();
  snappy::Test_SnappyCorruption_TruncatedVarint();
  snappy::Test_SnappyCorruption_UnterminatedVarint();
  snappy::Test_Snappy_ReadPastEndOfBuffer();
  snappy::Test_Snappy_FindMatchLength();
  snappy::Test_Snappy_FindMatchLengthRandom();
  fprintf(stderr, "All tests passed.\n");

  return 0;
}

#endif  // HAVE_GTEST

// For main().
namespace snappy {

static void CompressFile(const char* fname);
static void UncompressFile(const char* fname);
static void MeasureFile(const char* fname);

}  // namespace

using snappy::CompressFile;
using snappy::UncompressFile;
using snappy::MeasureFile;

#endif  // UTIL_SNAPPY_OPENSOURCE_SNAPPY_TEST_H_