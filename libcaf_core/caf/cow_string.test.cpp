// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/cow_string.hpp"

#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

using std::make_tuple;
using std::string;
using std::tuple;

using namespace caf;
using namespace std::literals;

namespace {

SCENARIO("default constructed COW strings are empty") {
  WHEN("default-constructing a COW tuple") {
    cow_string str;
    THEN("the string is empty") {
      check(str.empty());
      check_eq(str.size(), 0u);
      check_eq(str.length(), 0u);
      check_eq(str.begin(), str.end());
      check_eq(str.rbegin(), str.rend());
    }
    AND_THEN("the reference count is exactly 1") {
      check(str.unique());
    }
  }
}

SCENARIO("COW string are constructible from STD strings") {
  WHEN("copy-constructing a COW string from an STD string") {
    auto std_str = "hello world"s;
    auto str = cow_string{std_str};
    THEN("the COW string contains a copy of the original string content") {
      check(!str.empty());
      check_eq(str.size(), std_str.size());
      check_eq(str.length(), std_str.length());
      check_ne(str.begin(), str.end());
      check_ne(str.rbegin(), str.rend());
      check_eq(str, std_str);
    }
    AND_THEN("the reference count is exactly 1") {
      check(str.unique());
    }
  }
  WHEN("move-constructing a COW string from an STD string") {
    auto std_str = "hello world"s;
    auto str = cow_string{std::move(std_str)};
    THEN("the COW string contains the original string content") {
      check(!str.empty());
      check_ne(str.begin(), str.end());
      check_ne(str.rbegin(), str.rend());
      check_ne(str, std_str); // NOLINT(bugprone-use-after-move)
      check_eq(str, "hello world");
    }
    AND_THEN("the reference count is exactly 1") {
      check(str.unique());
    }
  }
}

SCENARIO("copying COW strings makes shallow copies") {
  WHEN("copy-constructing a COW string from an another COW string") {
    auto str1 = cow_string{"hello world"s};
    auto str2 = str1;
    THEN("both COW strings point to the same data") {
      check_eq(str1.data(), str2.data());
    }
    AND_THEN("the reference count is at least 2") {
      check(!str1.unique());
      check(!str2.unique());
    }
  }
}

SCENARIO("COW strings detach their content when becoming unshared") {
  WHEN("copy-constructing a COW string from an another COW string") {
    auto str1 = cow_string{"hello world"s};
    auto str2 = str1;
    THEN("writing to the original does not change the copy") {
      str1.unshared() = "foobar";
      check_eq(str1, "foobar");
      check_eq(str2, "hello world");
      check(str1.unique());
      check(str2.unique());
    }
  }
}

} // namespace
