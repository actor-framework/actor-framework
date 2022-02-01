// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE cow_string

#include "caf/cow_string.hpp"

#include "core-test.hpp"

using std::make_tuple;
using std::string;
using std::tuple;

using namespace caf;
using namespace std::literals;

SCENARIO("default constructed COW strings are empty") {
  WHEN("default-constructing a COW tuple") {
  	cow_string str;
    THEN("the string is empty") {
      CHECK(str.empty());
      CHECK_EQ(str.size(), 0u);
      CHECK_EQ(str.length(), 0u);
      CHECK_EQ(str.begin(), str.end());
      CHECK_EQ(str.rbegin(), str.rend());
    }
    AND("the reference count is exactly 1") {
      CHECK(str.unique());
    }
  }
}

SCENARIO("COW string are constructible from STD strings") {
  WHEN("copy-constructing a COW string from an STD string") {
    auto std_str = "hello world"s;
    auto str = cow_string{std_str};
    THEN("the COW string contains a copy of the original string content") {
      CHECK(!str.empty());
      CHECK_EQ(str.size(), std_str.size());
      CHECK_EQ(str.length(), std_str.length());
      CHECK_NE(str.begin(), str.end());
      CHECK_NE(str.rbegin(), str.rend());
      CHECK_EQ(str, std_str);
    }
    AND("the reference count is exactly 1") {
      CHECK(str.unique());
    }
  }
  WHEN("move-constructing a COW string from an STD string") {
    auto std_str = "hello world"s;
    auto str = cow_string{std::move(std_str)};
    THEN("the COW string contains the original string content") {
      CHECK(!str.empty());
      CHECK_NE(str.begin(), str.end());
      CHECK_NE(str.rbegin(), str.rend());
      CHECK_NE(str, std_str);
      CHECK_EQ(str, "hello world");
    }
    AND("the reference count is exactly 1") {
      CHECK(str.unique());
    }
  }
}

SCENARIO("copying COW strings makes shallow copies") {
  WHEN("copy-constructing a COW string from an another COW string") {
    auto str1 = cow_string{"hello world"s};
    auto str2 = str1;
    THEN("both COW strings point to the same data") {
      CHECK_EQ(str1.data(), str2.data());
    }
    AND("the reference count is at least 2") {
      CHECK(!str1.unique());
      CHECK(!str2.unique());
    }
  }
}

SCENARIO("COW strings detach their content when becoming unshared") {
  WHEN("copy-constructing a COW string from an another COW string") {
    auto str1 = cow_string{"hello world"s};
    auto str2 = str1;
    THEN("writing to the original does not change the copy") {
      str1.unshared() = "foobar";
      CHECK_EQ(str1, "foobar");
      CHECK_EQ(str2, "hello world");
      CHECK(str1.unique());
      CHECK(str2.unique());
    }
  }
}
