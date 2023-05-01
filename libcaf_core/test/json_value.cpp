// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE json_value

#include "caf/json_value.hpp"

#include "core-test.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/json_array.hpp"
#include "caf/json_object.hpp"

using namespace caf;
using namespace std::literals;

namespace {

std::string printed(const json_value& val) {
  std::string result;
  val.print_to(result, 2);
  return result;
}

} // namespace

TEST_CASE("default-constructed") {
  auto val = json_value{};
  CHECK(val.is_null());
  CHECK(!val.is_undefined());
  CHECK(!val.is_integer());
  CHECK(!val.is_integer());
  CHECK(!val.is_double());
  CHECK(!val.is_number());
  CHECK(!val.is_bool());
  CHECK(!val.is_string());
  CHECK(!val.is_array());
  CHECK(!val.is_object());
  CHECK_EQ(val.to_integer(), 0);
  CHECK_EQ(val.to_unsigned(), 0u);
  CHECK_EQ(val.to_double(), 0.0);
  CHECK_EQ(val.to_bool(), false);
  CHECK_EQ(val.to_string(), ""sv);
  CHECK_EQ(val.to_object().size(), 0u);
  CHECK_EQ(to_string(val), "null");
  CHECK_EQ(printed(val), "null");
  CHECK_EQ(deep_copy(val), val);
}

TEST_CASE("from undefined") {
  auto val = json_value::undefined();
  CHECK(!val.is_null());
  CHECK(val.is_undefined());
  CHECK(!val.is_integer());
  CHECK(!val.is_integer());
  CHECK(!val.is_double());
  CHECK(!val.is_number());
  CHECK(!val.is_bool());
  CHECK(!val.is_string());
  CHECK(!val.is_array());
  CHECK(!val.is_object());
  CHECK_EQ(val.to_integer(), 0);
  CHECK_EQ(val.to_unsigned(), 0u);
  CHECK_EQ(val.to_double(), 0.0);
  CHECK_EQ(val.to_bool(), false);
  CHECK_EQ(val.to_string(), ""sv);
  CHECK_EQ(val.to_object().size(), 0u);
  CHECK_EQ(to_string(val), "null");
  CHECK_EQ(printed(val), "null");
  CHECK_EQ(deep_copy(val), val);
}

TEST_CASE("from negative integer") {
  auto val = unbox(json_value::parse("-1"));
  CHECK(!val.is_null());
  CHECK(!val.is_undefined());
  CHECK(val.is_integer());
  CHECK(!val.is_unsigned());
  CHECK(!val.is_double());
  CHECK(val.is_number());
  CHECK(!val.is_bool());
  CHECK(!val.is_string());
  CHECK(!val.is_array());
  CHECK(!val.is_object());
  CHECK_EQ(val.to_integer(), -1);
  CHECK_EQ(val.to_unsigned(), 0u);
  CHECK_EQ(val.to_double(), -1.0);
  CHECK_EQ(val.to_bool(), false);
  CHECK_EQ(val.to_string(), ""sv);
  CHECK_EQ(val.to_object().size(), 0u);
  CHECK_EQ(to_string(val), "-1");
  CHECK_EQ(printed(val), "-1");
  CHECK_EQ(deep_copy(val), val);
}

TEST_CASE("from small integer") {
  // A small integer can be represented as both int64_t and uint64_t.
  auto val = unbox(json_value::parse("42"));
  CHECK(!val.is_null());
  CHECK(!val.is_undefined());
  CHECK(val.is_integer());
  CHECK(val.is_unsigned());
  CHECK(!val.is_double());
  CHECK(val.is_number());
  CHECK(!val.is_bool());
  CHECK(!val.is_string());
  CHECK(!val.is_array());
  CHECK(!val.is_object());
  CHECK_EQ(val.to_integer(), 42);
  CHECK_EQ(val.to_unsigned(), 42u);
  CHECK_EQ(val.to_double(), 42.0);
  CHECK_EQ(val.to_bool(), false);
  CHECK_EQ(val.to_string(), ""sv);
  CHECK_EQ(val.to_object().size(), 0u);
  CHECK_EQ(to_string(val), "42");
  CHECK_EQ(printed(val), "42");
  CHECK_EQ(deep_copy(val), val);
}

TEST_CASE("from UINT64_MAX") {
  auto val = unbox(json_value::parse("18446744073709551615"));
  CHECK(!val.is_null());
  CHECK(!val.is_undefined());
  CHECK(!val.is_integer());
  CHECK(val.is_unsigned());
  CHECK(!val.is_double());
  CHECK(val.is_number());
  CHECK(!val.is_bool());
  CHECK(!val.is_string());
  CHECK(!val.is_array());
  CHECK(!val.is_object());
  CHECK_EQ(val.to_integer(), 0);
  CHECK_EQ(val.to_unsigned(), UINT64_MAX);
  CHECK_EQ(val.to_double(), static_cast<double>(UINT64_MAX));
  CHECK_EQ(val.to_bool(), false);
  CHECK_EQ(val.to_string(), ""sv);
  CHECK_EQ(val.to_object().size(), 0u);
  CHECK_EQ(to_string(val), "18446744073709551615");
  CHECK_EQ(printed(val), "18446744073709551615");
  CHECK_EQ(deep_copy(val), val);
}

TEST_CASE("from double") {
  auto val = unbox(json_value::parse("42.0"));
  CHECK(!val.is_null());
  CHECK(!val.is_undefined());
  CHECK(!val.is_integer());
  CHECK(val.is_double());
  CHECK(val.is_number());
  CHECK(!val.is_bool());
  CHECK(!val.is_string());
  CHECK(!val.is_array());
  CHECK(!val.is_object());
  CHECK_EQ(val.to_integer(), 42);
  CHECK_EQ(val.to_unsigned(), 42u);
  CHECK_EQ(val.to_double(), 42.0);
  CHECK_EQ(val.to_bool(), false);
  CHECK_EQ(val.to_string(), ""sv);
  CHECK_EQ(val.to_object().size(), 0u);
  CHECK_EQ(to_string(val), "42");
  CHECK_EQ(printed(val), "42");
  CHECK_EQ(deep_copy(val), val);
}

TEST_CASE("from bool") {
  auto val = unbox(json_value::parse("true"));
  CHECK(!val.is_null());
  CHECK(!val.is_undefined());
  CHECK(!val.is_integer());
  CHECK(!val.is_double());
  CHECK(!val.is_number());
  CHECK(val.is_bool());
  CHECK(!val.is_string());
  CHECK(!val.is_array());
  CHECK(!val.is_object());
  CHECK_EQ(val.to_integer(), 0);
  CHECK_EQ(val.to_unsigned(), 0u);
  CHECK_EQ(val.to_double(), 0.0);
  CHECK_EQ(val.to_bool(), true);
  CHECK_EQ(val.to_string(), ""sv);
  CHECK_EQ(val.to_object().size(), 0u);
  CHECK_EQ(to_string(val), "true");
  CHECK_EQ(printed(val), "true");
  CHECK_EQ(deep_copy(val), val);
}

TEST_CASE("from string") {
  auto val = unbox(json_value::parse(R"_("Hello, world!")_"));
  CHECK(!val.is_null());
  CHECK(!val.is_undefined());
  CHECK(!val.is_integer());
  CHECK(!val.is_double());
  CHECK(!val.is_number());
  CHECK(!val.is_bool());
  CHECK(val.is_string());
  CHECK(!val.is_array());
  CHECK(!val.is_object());
  CHECK_EQ(val.to_integer(), 0);
  CHECK_EQ(val.to_unsigned(), 0u);
  CHECK_EQ(val.to_double(), 0.0);
  CHECK_EQ(val.to_bool(), false);
  CHECK_EQ(val.to_string(), "Hello, world!"sv);
  CHECK_EQ(val.to_object().size(), 0u);
  CHECK_EQ(to_string(val), R"_("Hello, world!")_");
  CHECK_EQ(printed(val), R"_("Hello, world!")_");
  CHECK_EQ(deep_copy(val), val);
}

TEST_CASE("from empty array") {
  auto val = unbox(json_value::parse("[]"));
  CHECK(!val.is_null());
  CHECK(!val.is_undefined());
  CHECK(!val.is_integer());
  CHECK(!val.is_double());
  CHECK(!val.is_number());
  CHECK(!val.is_bool());
  CHECK(!val.is_string());
  CHECK(val.is_array());
  CHECK(!val.is_object());
  CHECK_EQ(val.to_integer(), 0);
  CHECK_EQ(val.to_unsigned(), 0u);
  CHECK_EQ(val.to_double(), 0.0);
  CHECK_EQ(val.to_bool(), false);
  CHECK_EQ(val.to_string(), ""sv);
  CHECK_EQ(val.to_array().size(), 0u);
  CHECK_EQ(val.to_object().size(), 0u);
  CHECK_EQ(to_string(val), "[]");
  CHECK_EQ(printed(val), "[]");
  CHECK_EQ(deep_copy(val), val);
}

TEST_CASE("from array of size 1") {
  auto val = unbox(json_value::parse("[1]"));
  CHECK(!val.is_null());
  CHECK(!val.is_undefined());
  CHECK(!val.is_integer());
  CHECK(!val.is_double());
  CHECK(!val.is_number());
  CHECK(!val.is_bool());
  CHECK(!val.is_string());
  CHECK(val.is_array());
  CHECK(!val.is_object());
  CHECK_EQ(val.to_integer(), 0);
  CHECK_EQ(val.to_unsigned(), 0u);
  CHECK_EQ(val.to_double(), 0.0);
  CHECK_EQ(val.to_bool(), false);
  CHECK_EQ(val.to_string(), ""sv);
  CHECK_EQ(val.to_array().size(), 1u);
  CHECK_EQ(val.to_object().size(), 0u);
  CHECK_EQ(to_string(val), "[1]");
  CHECK_EQ(printed(val), "[\n  1\n]");
  CHECK_EQ(deep_copy(val), val);
}

TEST_CASE("from array of size 3") {
  auto val = unbox(json_value::parse("[1, 2, 3]"));
  CHECK(!val.is_null());
  CHECK(!val.is_undefined());
  CHECK(!val.is_integer());
  CHECK(!val.is_double());
  CHECK(!val.is_number());
  CHECK(!val.is_bool());
  CHECK(!val.is_string());
  CHECK(val.is_array());
  CHECK(!val.is_object());
  CHECK_EQ(val.to_integer(), 0);
  CHECK_EQ(val.to_unsigned(), 0u);
  CHECK_EQ(val.to_double(), 0.0);
  CHECK_EQ(val.to_bool(), false);
  CHECK_EQ(val.to_string(), ""sv);
  CHECK_EQ(val.to_array().size(), 3u);
  CHECK_EQ(val.to_object().size(), 0u);
  CHECK_EQ(to_string(val), "[1, 2, 3]");
  CHECK_EQ(printed(val), "[\n  1,\n  2,\n  3\n]");
  CHECK_EQ(deep_copy(val), val);
}

TEST_CASE("from empty object") {
  auto val = unbox(json_value::parse("{}"));
  CHECK(!val.is_null());
  CHECK(!val.is_undefined());
  CHECK(!val.is_integer());
  CHECK(!val.is_double());
  CHECK(!val.is_number());
  CHECK(!val.is_bool());
  CHECK(!val.is_string());
  CHECK(!val.is_array());
  CHECK(val.is_object());
  CHECK_EQ(val.to_integer(), 0);
  CHECK_EQ(val.to_unsigned(), 0u);
  CHECK_EQ(val.to_double(), 0.0);
  CHECK_EQ(val.to_bool(), false);
  CHECK_EQ(val.to_string(), ""sv);
  CHECK_EQ(val.to_object().size(), 0u);
  CHECK_EQ(to_string(val), "{}");
  CHECK_EQ(printed(val), "{}");
  CHECK_EQ(deep_copy(val), val);
}

TEST_CASE("from non-empty object") {
  auto val = unbox(json_value::parse(R"_({"foo": "bar"})_"));
  CHECK(!val.is_null());
  CHECK(!val.is_undefined());
  CHECK(!val.is_integer());
  CHECK(!val.is_double());
  CHECK(!val.is_number());
  CHECK(!val.is_bool());
  CHECK(!val.is_string());
  CHECK(!val.is_array());
  CHECK(val.is_object());
  CHECK_EQ(val.to_integer(), 0);
  CHECK_EQ(val.to_unsigned(), 0u);
  CHECK_EQ(val.to_double(), 0.0);
  CHECK_EQ(val.to_bool(), false);
  CHECK_EQ(val.to_string(), ""sv);
  CHECK_EQ(val.to_object().size(), 1u);
  CHECK_EQ(to_string(val), R"_({"foo": "bar"})_");
  CHECK_EQ(printed(val), "{\n  \"foo\": \"bar\"\n}");
  CHECK_EQ(deep_copy(val), val);
}
