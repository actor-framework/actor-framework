// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE json_object

#include "caf/json_object.hpp"

#include "core-test.hpp"

#include "caf/json_array.hpp"
#include "caf/json_value.hpp"

using namespace caf;
using namespace std::literals;

TEST_CASE("default-constructed") {
  auto obj = json_object{};
  CHECK(obj.empty());
  CHECK(obj.is_empty());
  CHECK(obj.begin() == obj.end());
  CHECK_EQ(obj.size(), 0u);
  CHECK(obj.value("foo").is_undefined());
  CHECK_EQ(deep_copy(obj), obj);
}

TEST_CASE("from empty object") {
  auto obj = json_value::parse("{}")->to_object();
  CHECK(obj.empty());
  CHECK(obj.is_empty());
  CHECK(obj.begin() == obj.end());
  CHECK_EQ(obj.size(), 0u);
  CHECK(obj.value("foo").is_undefined());
  CHECK_EQ(deep_copy(obj), obj);
}

TEST_CASE("from non-empty object") {
  auto obj = json_value::parse(R"_({"a": "one", "b": 2})_")->to_object();
  CHECK(!obj.empty());
  CHECK(!obj.is_empty());
  CHECK(obj.begin() != obj.end());
  REQUIRE_EQ(obj.size(), 2u);
  CHECK_EQ(obj.begin().key(), "a");
  CHECK_EQ(obj.begin().value().to_string(), "one");
  CHECK_EQ(obj.value("a").to_string(), "one");
  CHECK_EQ(obj.value("b").to_integer(), 2);
  CHECK(obj.value("c").is_undefined());
  std::vector<std::pair<std::string_view, json_value>> vals;
  for (const auto& val : obj) {
    vals.emplace_back(val);
  }
  REQUIRE_EQ(vals.size(), 2u);
  CHECK_EQ(vals[0].first, "a");
  CHECK_EQ(vals[0].second.to_string(), "one");
  CHECK_EQ(vals[1].first, "b");
  CHECK_EQ(vals[1].second.to_integer(), 2);
  CHECK_EQ(deep_copy(obj), obj);
}
