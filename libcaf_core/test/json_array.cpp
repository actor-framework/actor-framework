// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE json_array

#include "caf/json_array.hpp"

#include "core-test.hpp"

#include "caf/json_array.hpp"
#include "caf/json_value.hpp"

using namespace caf;
using namespace std::literals;

TEST_CASE("default-constructed") {
  auto arr = json_array{};
  CHECK(arr.empty());
  CHECK(arr.is_empty());
  CHECK(arr.begin() == arr.end());
  CHECK_EQ(arr.size(), 0u);
  CHECK_EQ(deep_copy(arr), arr);
}

TEST_CASE("from empty array") {
  auto arr = json_value::parse("[]")->to_array();
  CHECK(arr.empty());
  CHECK(arr.is_empty());
  CHECK(arr.begin() == arr.end());
  CHECK_EQ(arr.size(), 0u);
  CHECK_EQ(deep_copy(arr), arr);
}

TEST_CASE("from non-empty array") {
  auto arr = json_value::parse(R"_([1, "two", 3.0])_")->to_array();
  CHECK(!arr.empty());
  CHECK(!arr.is_empty());
  CHECK(arr.begin() != arr.end());
  REQUIRE_EQ(arr.size(), 3u);
  CHECK_EQ((*arr.begin()).to_integer(), 1);
  std::vector<json_value> vals;
  for (const auto& val : arr) {
    vals.emplace_back(val);
  }
  REQUIRE_EQ(vals.size(), 3u);
  CHECK_EQ(vals[0].to_integer(), 1);
  CHECK_EQ(vals[1].to_string(), "two");
  CHECK_EQ(vals[2].to_double(), 3.0);
  CHECK_EQ(deep_copy(arr), arr);
}
