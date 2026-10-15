/**
 * @file   test_invalid_handles.cc
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
 * Tests that the C API rejects NULL handles and reports which kind of handle
 */

#include "catch.h"
#include "tiledb.h"

#include <string>

static void clear_errmsg() {
  tiledb_errmsg[0] = '\0';
}

TEST_CASE("A NULL context is reported as an invalid context", "[invalid_handles]") {
  clear_errmsg();
  CHECK(tiledb_workspace_create(NULL, "unused") == TILEDB_ERR);
  CHECK(std::string(tiledb_errmsg) == TILEDB_ERRMSG + "Invalid TileDB context");
}

TEST_CASE("A NULL array is reported as an invalid array", "[invalid_handles]") {
  clear_errmsg();
  CHECK(tiledb_array_finalize(NULL) == TILEDB_ERR);
  CHECK(std::string(tiledb_errmsg) == TILEDB_ERRMSG + "Invalid TileDB array");
}

TEST_CASE("A NULL array iterator is reported as an invalid array iterator", "[invalid_handles]") {
  clear_errmsg();
  const void* value = NULL;
  size_t value_size = 0;
  CHECK(tiledb_array_iterator_get_value(NULL, 0, &value, &value_size) == TILEDB_ERR);
  CHECK(std::string(tiledb_errmsg) == TILEDB_ERRMSG + "Invalid TileDB array iterator");
}

TEST_CASE("A NULL metadata is reported as an invalid metadata", "[invalid_handles]") {
  clear_errmsg();
  CHECK(tiledb_metadata_finalize(NULL) == TILEDB_ERR);
  CHECK(std::string(tiledb_errmsg) == TILEDB_ERRMSG + "Invalid TileDB metadata");
}

TEST_CASE("A NULL metadata iterator is reported as an invalid metadata iterator", "[invalid_handles]") {
  clear_errmsg();
  CHECK(tiledb_metadata_iterator_finalize(NULL) == TILEDB_ERR);
  CHECK(std::string(tiledb_errmsg) == TILEDB_ERRMSG + "Invalid TileDB metadata iterator");
}
