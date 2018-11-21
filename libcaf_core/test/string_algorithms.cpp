/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE string_algorithms

#include "caf/string_algorithms.hpp"

#include "caf/test/dsl.hpp"

#include <string>
#include <vector>

using namespace caf;

namespace {

using str_list = std::vector<std::string>;

str_list split(string_view str) {
  str_list result;
  caf::split(result, str, ",");
  return result;
}

str_list compressed_split(string_view str) {
  str_list result;
  caf::split(result, str, ",", token_compress_on);
  return result;
}

std::string join(str_list vec) {
  return caf::join(vec, ",");
}

} // namespace <anonymous>

CAF_TEST(splitting) {
  CAF_CHECK_EQUAL(split(""), str_list({""}));
  CAF_CHECK_EQUAL(split(","), str_list({"", ""}));
  CAF_CHECK_EQUAL(split(",,"), str_list({"", "", ""}));
  CAF_CHECK_EQUAL(split(",,,"), str_list({"", "", "", ""}));
  CAF_CHECK_EQUAL(split("a,b,c"), str_list({"a", "b", "c"}));
  CAF_CHECK_EQUAL(split("a,,b,c,"), str_list({"a", "", "b", "c", ""}));
}

CAF_TEST(compressed splitting) {
  CAF_CHECK_EQUAL(compressed_split(""), str_list({}));
  CAF_CHECK_EQUAL(compressed_split(","), str_list({}));
  CAF_CHECK_EQUAL(compressed_split(",,"), str_list({}));
  CAF_CHECK_EQUAL(compressed_split(",,,"), str_list({}));
  CAF_CHECK_EQUAL(compressed_split("a,b,c"), str_list({"a", "b", "c"}));
  CAF_CHECK_EQUAL(compressed_split("a,,b,c,"), str_list({"a", "b", "c"}));
}

CAF_TEST(joining) {
  CAF_CHECK_EQUAL(join({}), "");
  CAF_CHECK_EQUAL(join({""}), "");
  CAF_CHECK_EQUAL(join({"", ""}), ",");
  CAF_CHECK_EQUAL(join({"", "", ""}), ",,");
  CAF_CHECK_EQUAL(join({"a"}), "a");
  CAF_CHECK_EQUAL(join({"a", "b"}), "a,b");
  CAF_CHECK_EQUAL(join({"a", "b", "c"}), "a,b,c");
}

CAF_TEST(starts with) {
  CAF_CHECK(starts_with("foobar", "f"));
  CAF_CHECK(starts_with("foobar", "fo"));
  CAF_CHECK(starts_with("foobar", "fooba"));
  CAF_CHECK(starts_with("foobar", "foobar"));
  CAF_CHECK(!starts_with("foobar", "o"));
  CAF_CHECK(!starts_with("foobar", "fa"));
  CAF_CHECK(!starts_with("foobar", "foobaro"));
}

CAF_TEST(ends with) {
  CAF_CHECK(ends_with("foobar", "r"));
  CAF_CHECK(ends_with("foobar", "ar"));
  CAF_CHECK(ends_with("foobar", "oobar"));
  CAF_CHECK(ends_with("foobar", "foobar"));
  CAF_CHECK(!ends_with("foobar", "a"));
  CAF_CHECK(!ends_with("foobar", "car"));
  CAF_CHECK(!ends_with("foobar", "afoobar"));
}
