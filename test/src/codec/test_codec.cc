/**
 * @file   test_codec.cc
 *
 * @section LICENSE
 *
 * The MIT License
 *
 * @copyright Copyright (c) 2019-2020 Omics Data Automation, Inc.
 * @copyright Copyright (c) 2023 dātma, inc™
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 * @section DESCRIPTION
 *
 * Test compression/decompression tiledb classes
 */

#include "catch.h"
#include "codec.h"
#include "codec_gzip.h"
#include "tiledb.h"

#include <limits.h>

class TestCodecBasic : public Codec {
 public:
  using Codec::Codec;

  static Codec* createTestCodecBasic(const ArraySchema* array_schema, const int attribute_id, const bool is_offsets_compression) {
    return new TestCodecBasic(/*compression_level*/1);
  }
  
  int do_compress_tile(unsigned char* tile, size_t tile_size, void** tile_compressed, size_t& tile_compressed_size) {
    return TILEDB_CD_OK;
  }
  int do_decompress_tile(unsigned char* tile_compressed,  size_t tile_compressed_size, unsigned char* tile, size_t tile_size) {
    return TILEDB_CD_OK;
  }
};

TEST_CASE("Test codec static methods", "[codec_static]") {
  CHECK(Codec::get_default_level(TILEDB_GZIP) == TILEDB_COMPRESSION_LEVEL_GZIP);
  CHECK(Codec::get_default_level(INT_MAX) == -1);
}

TEST_CASE("Test codec basic", "[codec_basic]") {
  TestCodecBasic *codec_basic = new TestCodecBasic(0);
  
  void *dl_handle = codec_basic->get_dlopen_handle("non-existent-library");
  CHECK(dl_handle == NULL);
  CHECK(!codec_basic->get_dlerror().empty());

  dl_handle = codec_basic->get_dlopen_handle("non-existent-library", "5.2");
  CHECK(dl_handle == NULL);
  CHECK(!codec_basic->get_dlerror().empty());
  
  dl_handle = codec_basic->get_dlopen_handle("z");
  CHECK(dl_handle);
  CHECK(codec_basic->get_dlerror().empty());

  dl_handle = codec_basic->get_dlopen_handle("z", "1");
  CHECK(dl_handle);
  CHECK(codec_basic->get_dlerror().empty());
  
  dl_handle = codec_basic->get_dlopen_handle("z", "non-existent-version");
  CHECK(dl_handle == NULL);
  CHECK(!codec_basic->get_dlerror().empty());

  delete codec_basic;
}

TEST_CASE("Test zlib", "[codec-z]") {
  Codec* zlib = new CodecGzip(TILEDB_COMPRESSION_LEVEL_GZIP);
  unsigned char test_string[] = "HELLO";
  unsigned char* buffer;
  size_t buffer_size;

  CHECK(zlib->compress_tile(test_string, 0, (void **)(&buffer), buffer_size) == TILEDB_CD_OK);
  CHECK(zlib->compress_tile(test_string, 6, (void **)(&buffer), buffer_size) == TILEDB_CD_OK);

  unsigned char* decompressed_string =  (unsigned char*)malloc(6);
  CHECK(zlib->decompress_tile(buffer, buffer_size, decompressed_string, 6) == TILEDB_CD_OK);

  // Should get Z_BUF_ERROR
  CHECK(zlib->decompress_tile(buffer, buffer_size, decompressed_string, 4) == TILEDB_CD_ERR);
  CHECK(zlib->decompress_tile(buffer, 4, decompressed_string, 6) == TILEDB_CD_ERR);
  // Should get Z_DATA_ERROR
  CHECK(zlib->decompress_tile(test_string, 4, decompressed_string, 6) == TILEDB_CD_ERR);
   
  delete zlib;
}

#include "codec_lz4.h"

TEST_CASE("Test lz4", "[codec-lz4]") {
  // Try lz4 with default compression
  Codec* lz4 = new CodecLZ4(TILEDB_COMPRESSION_LEVEL_LZ4);
  unsigned char test_string[] = "HELLO";
  unsigned char* buffer;
  size_t buffer_size;

  CHECK(lz4->compress_tile(test_string, 0, (void **)(&buffer), buffer_size) == TILEDB_CD_OK);
  CHECK(lz4->compress_tile(test_string, 6, (void **)(&buffer), buffer_size) == TILEDB_CD_OK);

  unsigned char* decompressed_string =  (unsigned char*)malloc(6);
  CHECK(lz4->decompress_tile(buffer, buffer_size, decompressed_string, 6) == TILEDB_CD_OK);
  CHECK(strlen((char *)decompressed_string) == 5);
  CHECK(strcmp((char *)decompressed_string, (char *)test_string) == 0);

  delete lz4;

  // Try lz4 with acceleration=2
  lz4 = new CodecLZ4(2);

  CHECK(lz4->compress_tile(test_string, 0, (void **)(&buffer), buffer_size) == TILEDB_CD_OK);
  CHECK(lz4->compress_tile(test_string, 6, (void **)(&buffer), buffer_size) == TILEDB_CD_OK);

  memset(decompressed_string, 0, 6);
  CHECK(lz4->decompress_tile(buffer, buffer_size, decompressed_string, 6) == TILEDB_CD_OK);
  CHECK(strlen((char *)decompressed_string) == 5);
  CHECK(strcmp((char *)decompressed_string, (char *)test_string) == 0);

  free(decompressed_string);
  delete lz4;
}

#include "codec_zstd.h"

#include <string>
#include <thread>
#include <vector>

// Repetitive text, so that it compresses
static std::vector<unsigned char> make_compressible_tile() {
  std::vector<unsigned char> tile;
  for (int i = 0; i < 1000; ++i) {
    std::string line = "chr20\t" + std::to_string(60001 + i) + "\tA\t<NON_REF>\n";
    tile.insert(tile.end(), line.begin(), line.end());
  }
  return tile;
}

// Compresses and decompresses tile with a new codec at compression_level
static std::vector<unsigned char> zstd_round_trip(const std::vector<unsigned char>& tile, int compression_level) {
  CodecZStandard zstd(compression_level);
  std::vector<unsigned char> input(tile);
  unsigned char* compressed;
  size_t compressed_size;
  REQUIRE(zstd.compress_tile(input.data(), input.size(), (void **)(&compressed), compressed_size) == TILEDB_CD_OK);
  CHECK(compressed_size < tile.size());
  std::vector<unsigned char> decompressed(tile.size());
  REQUIRE(zstd.decompress_tile(compressed, compressed_size, decompressed.data(), decompressed.size()) == TILEDB_CD_OK);
  return decompressed;
}

TEST_CASE("zstd round-trips a tile at the default level", "[codec-zstd]") {
  auto tile = make_compressible_tile();
  CHECK(zstd_round_trip(tile, TILEDB_COMPRESSION_LEVEL_ZSTD) == tile);
}

TEST_CASE("zstd round-trips a tile at its fastest and strongest levels", "[codec-zstd]") {
  auto tile = make_compressible_tile();
  CHECK(zstd_round_trip(tile, 1) == tile);
  CHECK(zstd_round_trip(tile, 22) == tile);
}

TEST_CASE("zstd decompression fails when the output buffer is too small", "[codec-zstd]") {
  auto tile = make_compressible_tile();
  CodecZStandard zstd(TILEDB_COMPRESSION_LEVEL_ZSTD);
  unsigned char* compressed;
  size_t compressed_size;
  REQUIRE(zstd.compress_tile(tile.data(), tile.size(), (void **)(&compressed), compressed_size) == TILEDB_CD_OK);
  std::vector<unsigned char> decompressed(tile.size() - 1);
  CHECK(zstd.decompress_tile(compressed, compressed_size, decompressed.data(), decompressed.size()) == TILEDB_CD_ERR);
}

TEST_CASE("zstd decompression fails on data that is not zstd", "[codec-zstd]") {
  auto tile = make_compressible_tile();
  CodecZStandard zstd(TILEDB_COMPRESSION_LEVEL_ZSTD);
  std::vector<unsigned char> decompressed(tile.size());
  CHECK(zstd.decompress_tile(tile.data(), tile.size(), decompressed.data(), decompressed.size()) == TILEDB_CD_ERR);
}

// Each thread has its own compression and decompression contexts, freed when the thread exits
TEST_CASE("zstd round-trips tiles on threads that then exit", "[codec-zstd]") {
  auto tile = make_compressible_tile();
  std::vector<std::vector<unsigned char>> results(4);
  std::vector<std::thread> threads;
  for (auto& result : results) {
    threads.emplace_back([&tile, &result]() {
      CodecZStandard zstd(TILEDB_COMPRESSION_LEVEL_ZSTD);
      std::vector<unsigned char> input(tile);
      unsigned char* compressed;
      size_t compressed_size;
      if (zstd.compress_tile(input.data(), input.size(), (void **)(&compressed), compressed_size) != TILEDB_CD_OK) return;
      result.resize(tile.size());
      if (zstd.decompress_tile(compressed, compressed_size, result.data(), result.size()) != TILEDB_CD_OK) result.clear();
    });
  }
  for (auto& thread : threads) thread.join();
  for (const auto& result : results) CHECK(result == tile);
}

TEST_CASE("Test plugin", "[codec-plugin]") {
  int compression_type = 15;
  CHECK(Codec::register_codec(compression_type, &TestCodecBasic::createTestCodecBasic) == TILEDB_CD_OK);
  CHECK(Codec::register_codec(compression_type, &TestCodecBasic::createTestCodecBasic) == TILEDB_CD_ERR);
  
  ArraySchema array_schema(NULL);
  const char* attributes[1] = {"this_attribute"};
  array_schema.set_attributes(const_cast<char **>(attributes), 1);
  const int compression[2] = {compression_type, TILEDB_GZIP};
  array_schema.set_compression(const_cast<int *>(compression));
  CHECK(Codec::create(&array_schema, 0, false));
}

