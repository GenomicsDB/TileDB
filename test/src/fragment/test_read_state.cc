/**
 * @file   test_read_state.cc
 *
 * @section LICENSE
 *
 * The MIT License
 *
 * @copyright Copyright (c) 2026 Tim Fennell
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
 * Tests how ReadState reads tiles with the mmap read method: the cells it returns
 * and the attribute files it holds open across tile reads
 */

#include "catch.h"
#include "tiledb.h"

#include <dirent.h>
#include <stdlib.h>
#include <string>
#include <vector>

/** Number of entries in /dev/fd: the process's open descriptors plus a constant few. */
static int count_open_file_descriptors() {
  int count = 0;
  DIR* dir = opendir("/dev/fd");
  REQUIRE(dir != NULL);
  while(readdir(dir) != NULL)
    ++count;
  closedir(dir);
  return count;
}

/**
 * A one-fragment sparse array with a GZIP-compressed, an uncompressed and a variable-sized
 * attribute, written in tiles of 10 cells so that a read spans many tiles.
 */
class ReadStateFixture : TempDir {
 public:
  static constexpr int num_cells = 100;

  ReadStateFixture() {
    unsetenv("TILEDB_MAX_CACHED_READ_FILE_HANDLES");
    REQUIRE(tiledb_ctx_init(&tiledb_ctx_, NULL) == TILEDB_OK);
    REQUIRE(tiledb_workspace_create(tiledb_ctx_, workspace_.c_str()) == TILEDB_OK);
    create_array();
    write_array();
  }

  ~ReadStateFixture() {
    unsetenv("TILEDB_MAX_CACHED_READ_FILE_HANDLES");
    CHECK(tiledb_ctx_finalize(tiledb_ctx_) == TILEDB_OK);
  }

  static std::string var_value(int i) {
    return "value-" + std::to_string(i);
  }

  /** The cells read by one open array, and the descriptors it held open while open. */
  struct ReadResult {
    std::vector<int32_t> gzip_values;
    std::vector<int32_t> plain_values;
    std::vector<std::string> var_values;
    std::vector<int32_t> coords;
    int descriptors_held_while_open;
    int descriptors_held_after_finalize;
  };

  /** Reads every cell of the array, counting open descriptors before, during and after. */
  ReadResult read_array() {
    ReadResult result;
    int before = count_open_file_descriptors();

    TileDB_Array* tiledb_array;
    REQUIRE(tiledb_array_init(tiledb_ctx_, &tiledb_array, array_name_.c_str(), TILEDB_ARRAY_READ,
                              NULL, NULL, 0) == TILEDB_OK);

    std::vector<int32_t> gzip_values(num_cells), plain_values(num_cells), coords(num_cells);
    std::vector<size_t> var_offsets(num_cells);
    std::vector<char> var_chars(num_cells * 16);
    void* buffers[] = { gzip_values.data(), plain_values.data(), var_offsets.data(), var_chars.data(),
                        coords.data() };
    size_t buffer_sizes[] = { gzip_values.size() * sizeof(int32_t), plain_values.size() * sizeof(int32_t),
                              var_offsets.size() * sizeof(size_t), var_chars.size(),
                              coords.size() * sizeof(int32_t) };
    REQUIRE(tiledb_array_read(tiledb_array, buffers, buffer_sizes) == TILEDB_OK);
    result.descriptors_held_while_open = count_open_file_descriptors() - before;
    REQUIRE(tiledb_array_finalize(tiledb_array) == TILEDB_OK);
    result.descriptors_held_after_finalize = count_open_file_descriptors() - before;

    int cells_read = buffer_sizes[0] / sizeof(int32_t);
    result.gzip_values.assign(gzip_values.begin(), gzip_values.begin() + cells_read);
    result.plain_values.assign(plain_values.begin(), plain_values.begin() + cells_read);
    result.coords.assign(coords.begin(), coords.begin() + cells_read);
    for(int i = 0; i < cells_read; ++i) {
      size_t end = (i + 1 < cells_read) ? var_offsets[i + 1] : buffer_sizes[3];
      result.var_values.emplace_back(&var_chars[var_offsets[i]], end - var_offsets[i]);
    }
    return result;
  }

  /** Checks that every cell written was read back unchanged. */
  static void check_cells(const ReadResult& result) {
    REQUIRE(result.coords.size() == num_cells);
    for(int i = 0; i < num_cells; ++i) {
      CHECK(result.coords[i] == i);
      CHECK(result.gzip_values[i] == i);
      CHECK(result.plain_values[i] == 2 * i);
      CHECK(result.var_values[i] == var_value(i));
    }
  }

 private:
  const std::string workspace_ = get_temp_dir() + "/read_state_test_ws/";
  const std::string array_name_ = workspace_ + "sparse_array";
  TileDB_CTX* tiledb_ctx_;

  void create_array() {
    const char* attributes[] = { "gzip", "plain", "var" };
    const char* dimensions[] = { "X" };
    int32_t domain[] = { 0, num_cells - 1 };
    int32_t tile_extents[] = { 10 };
    const int types[] = { TILEDB_INT32, TILEDB_INT32, TILEDB_CHAR, TILEDB_INT32 };
    const int cell_val_num[] = { 1, 1, TILEDB_VAR_NUM };
    const int compression[] = { TILEDB_GZIP, TILEDB_NO_COMPRESSION, TILEDB_GZIP, TILEDB_GZIP };

    TileDB_ArraySchema array_schema;
    REQUIRE(tiledb_array_set_schema(&array_schema, array_name_.c_str(), attributes, 3,
                                    10, // capacity
                                    TILEDB_ROW_MAJOR, cell_val_num, compression,
                                    NULL, // compression level
                                    NULL, // offsets compression
                                    NULL, // offsets compression level
                                    0, // sparse
                                    dimensions, 1, domain, sizeof(domain), tile_extents, sizeof(tile_extents),
                                    TILEDB_ROW_MAJOR, types) == TILEDB_OK);
    REQUIRE(tiledb_array_create(tiledb_ctx_, &array_schema) == TILEDB_OK);
    REQUIRE(tiledb_array_free_schema(&array_schema) == TILEDB_OK);
  }

  void write_array() {
    std::vector<int32_t> gzip_values, plain_values, coords;
    std::vector<size_t> var_offsets;
    std::string var_chars;
    for(int i = 0; i < num_cells; ++i) {
      gzip_values.push_back(i);
      plain_values.push_back(2 * i);
      var_offsets.push_back(var_chars.size());
      var_chars += var_value(i);
      coords.push_back(i);
    }
    const void* buffers[] = { gzip_values.data(), plain_values.data(), var_offsets.data(), var_chars.data(),
                              coords.data() };
    size_t buffer_sizes[] = { gzip_values.size() * sizeof(int32_t), plain_values.size() * sizeof(int32_t),
                              var_offsets.size() * sizeof(size_t), var_chars.size(),
                              coords.size() * sizeof(int32_t) };

    TileDB_Array* tiledb_array;
    REQUIRE(tiledb_array_init(tiledb_ctx_, &tiledb_array, array_name_.c_str(), TILEDB_ARRAY_WRITE,
                              NULL, NULL, 0) == TILEDB_OK);
    REQUIRE(tiledb_array_write(tiledb_array, buffers, buffer_sizes) == TILEDB_OK);
    REQUIRE(tiledb_array_finalize(tiledb_array) == TILEDB_OK);
  }
};

TEST_CASE_METHOD(ReadStateFixture, "Reads return every cell while attribute files are held open",
                 "[read_state]") {
  check_cells(read_array());
}

TEST_CASE_METHOD(ReadStateFixture, "Reads return every cell when no attribute files may be held open",
                 "[read_state]") {
  setenv("TILEDB_MAX_CACHED_READ_FILE_HANDLES", "0", 1);
  check_cells(read_array());
}

TEST_CASE_METHOD(ReadStateFixture, "Attribute files are held open until the array is finalized",
                 "[read_state]") {
  setenv("TILEDB_MAX_CACHED_READ_FILE_HANDLES", "0", 1);
  ReadResult none_held = read_array();
  unsetenv("TILEDB_MAX_CACHED_READ_FILE_HANDLES");
  ReadResult held = read_array();

  // One file for each fixed-sized attribute and the coordinates, two for the variable-sized one
  CHECK(held.descriptors_held_while_open - none_held.descriptors_held_while_open == 5);
  CHECK(held.descriptors_held_after_finalize == 0);
  CHECK(none_held.descriptors_held_after_finalize == 0);
}

TEST_CASE_METHOD(ReadStateFixture, "No more attribute files are held open than TILEDB_MAX_CACHED_READ_FILE_HANDLES",
                 "[read_state]") {
  setenv("TILEDB_MAX_CACHED_READ_FILE_HANDLES", "0", 1);
  ReadResult none_held = read_array();
  setenv("TILEDB_MAX_CACHED_READ_FILE_HANDLES", "2", 1);
  ReadResult two_held = read_array();

  CHECK(two_held.descriptors_held_while_open - none_held.descriptors_held_while_open == 2);
  CHECK(two_held.descriptors_held_after_finalize == 0);
  check_cells(two_held);
}
