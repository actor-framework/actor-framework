// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE string_algorithms

#include "caf/string_algorithms.hpp"

#include "core-test.hpp"

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

} // namespace

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
