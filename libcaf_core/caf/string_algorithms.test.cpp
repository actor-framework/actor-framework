// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/string_algorithms.hpp"

#include "caf/test/test.hpp"

#include <string>
#include <vector>

using namespace caf;

namespace {

using str_list = std::vector<std::string>;

str_list split(std::string_view str) {
  str_list result;
  caf::split(result, str, ",");
  return result;
}

str_list compressed_split(std::string_view str) {
  str_list result;
  caf::split(result, str, ",", token_compress_on);
  return result;
}

std::string join(str_list vec) {
  return caf::join(vec, ",");
}

TEST("splitting") {
  check_eq(split(""), str_list{""});
  check_eq(split(","), str_list{"", ""});
  check_eq(split(",,"), str_list{"", "", ""});
  check_eq(split(",,,"), str_list{"", "", "", ""});
  check_eq(split("a,b,c"), str_list{"a", "b", "c"});
  check_eq(split("a,,b,c,"), str_list{"a", "", "b", "c", ""});
}

TEST("compressed splitting") {
  check_eq(compressed_split(""), str_list{});
  check_eq(compressed_split(","), str_list{});
  check_eq(compressed_split(",,"), str_list{});
  check_eq(compressed_split(",,,"), str_list{});
  check_eq(compressed_split("a,b,c"), str_list{"a", "b", "c"});
  check_eq(compressed_split("a,,b,c,"), str_list{"a", "b", "c"});
}

TEST("joining") {
  check_eq(join({}), "");
  check_eq(join({""}), "");
  check_eq(join({"", ""}), ",");
  check_eq(join({"", "", ""}), ",,");
  check_eq(join({"a"}), "a");
  check_eq(join({"a", "b"}), "a,b");
  check_eq(join({"a", "b", "c"}), "a,b,c");
}

TEST("starts with") {
  check(starts_with("foobar", "f"));
  check(starts_with("foobar", "foo"));
  check(starts_with("foobar", "fooba"));
  check(starts_with("foobar", "foobar"));
  check(!starts_with("foobar", "o"));
  check(!starts_with("foobar", "fa"));
  check(!starts_with("foobar", "foobaro"));
}

TEST("ends with") {
  check(ends_with("foobar", "r"));
  check(ends_with("foobar", "ar"));
  check(ends_with("foobar", "oobar"));
  check(ends_with("foobar", "foobar"));
  check(!ends_with("foobar", "a"));
  check(!ends_with("foobar", "car"));
  check(!ends_with("foobar", "afoobar"));
}

} // namespace
