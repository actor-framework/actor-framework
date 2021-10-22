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
  CHECK_EQ(split(""), str_list({""}));
  CHECK_EQ(split(","), str_list({"", ""}));
  CHECK_EQ(split(",,"), str_list({"", "", ""}));
  CHECK_EQ(split(",,,"), str_list({"", "", "", ""}));
  CHECK_EQ(split("a,b,c"), str_list({"a", "b", "c"}));
  CHECK_EQ(split("a,,b,c,"), str_list({"a", "", "b", "c", ""}));
}

CAF_TEST(compressed splitting) {
  CHECK_EQ(compressed_split(""), str_list({}));
  CHECK_EQ(compressed_split(","), str_list({}));
  CHECK_EQ(compressed_split(",,"), str_list({}));
  CHECK_EQ(compressed_split(",,,"), str_list({}));
  CHECK_EQ(compressed_split("a,b,c"), str_list({"a", "b", "c"}));
  CHECK_EQ(compressed_split("a,,b,c,"), str_list({"a", "b", "c"}));
}

CAF_TEST(joining) {
  CHECK_EQ(join({}), "");
  CHECK_EQ(join({""}), "");
  CHECK_EQ(join({"", ""}), ",");
  CHECK_EQ(join({"", "", ""}), ",,");
  CHECK_EQ(join({"a"}), "a");
  CHECK_EQ(join({"a", "b"}), "a,b");
  CHECK_EQ(join({"a", "b", "c"}), "a,b,c");
}

CAF_TEST(starts with) {
  CHECK(starts_with("foobar", "f"));
  CHECK(starts_with("foobar", "foo"));
  CHECK(starts_with("foobar", "fooba"));
  CHECK(starts_with("foobar", "foobar"));
  CHECK(!starts_with("foobar", "o"));
  CHECK(!starts_with("foobar", "fa"));
  CHECK(!starts_with("foobar", "foobaro"));
}

CAF_TEST(ends with) {
  CHECK(ends_with("foobar", "r"));
  CHECK(ends_with("foobar", "ar"));
  CHECK(ends_with("foobar", "oobar"));
  CHECK(ends_with("foobar", "foobar"));
  CHECK(!ends_with("foobar", "a"));
  CHECK(!ends_with("foobar", "car"));
  CHECK(!ends_with("foobar", "afoobar"));
}
