/**
 * @file   test_error_messages.cc
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
 * Tests that the C API's error message, tiledb_errmsg, holds long messages safely
 */

#include "catch.h"
#include "error.h"
#include "tiledb.h"

#include <cstring>
#include <string>

TEST_CASE("An error message naming a long array is kept whole", "[error_messages]") {
  TileDB_CTX* tiledb_ctx;
  REQUIRE(tiledb_ctx_init(&tiledb_ctx, NULL) == TILEDB_OK);

  // Within TILEDB_NAME_MAX_LEN, but too long for the 2000-byte message buffer there used to be
  std::string array_name = "/tmp/missing-" + std::string(4000, 'a');
  TileDB_Array* tiledb_array;
  CHECK(tiledb_array_init(tiledb_ctx, &tiledb_array, array_name.c_str(), TILEDB_ARRAY_READ,
                          NULL, NULL, 0) == TILEDB_ERR);
  CHECK(std::string(tiledb_errmsg).find(array_name) != std::string::npos);

  CHECK(tiledb_ctx_finalize(tiledb_ctx) == TILEDB_OK);
}

TEST_CASE("Error messages longer than the buffer are truncated to fit", "[error_messages]") {
  std::string errmsg(TILEDB_ERRMSG_MAX_LEN + 100, 'x');
  set_tiledb_errmsg(errmsg);
  CHECK(strlen(tiledb_errmsg) == TILEDB_ERRMSG_MAX_LEN - 1);
  CHECK(std::string(tiledb_errmsg) == errmsg.substr(0, TILEDB_ERRMSG_MAX_LEN - 1));
}
