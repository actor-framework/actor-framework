// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/json_value.hpp"

#include "caf/test/approx.hpp"
#include "caf/test/test.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/json_array.hpp"
#include "caf/json_object.hpp"

using namespace caf;
using namespace std::literals;

namespace {

template <class T>
T deep_copy(const T& val) {
  using namespace std::literals;
  caf::byte_buffer buf;
  {
    caf::binary_serializer sink{buf};
    if (!sink.apply(val)) {
      auto msg = "serialization failed in deep_copy: "s;
      msg += to_string(sink.get_error());
      CAF_RAISE_ERROR(msg.c_str());
    }
  }
  auto result = T{};
  {
    caf::binary_deserializer sink{buf};
    if (!sink.apply(result)) {
      auto msg = "deserialization failed in deep_copy: "s;
      msg += to_string(sink.get_error());
      CAF_RAISE_ERROR(msg.c_str());
    }
  }
  return result;
}

template <class T>
T unbox(caf::expected<T> x) {
  if (!x)
    test::runnable::current().fail("{}", to_string(x.error()));
  return std::move(*x);
}

std::string printed(const json_value& val) {
  std::string result;
  val.print_to(result, 2);
  return result;
}

} // namespace

TEST("default-constructed") {
  auto val = json_value{};
  check(val.is_null());
  check(!val.is_undefined());
  check(!val.is_integer());
  check(!val.is_integer());
  check(!val.is_double());
  check(!val.is_number());
  check(!val.is_bool());
  check(!val.is_string());
  check(!val.is_array());
  check(!val.is_object());
  check_eq(val.to_integer(), 0);
  check_eq(val.to_unsigned(), 0u);
  check_eq(val.to_double(), test::approx{0.0});
  check_eq(val.to_bool(), false);
  check_eq(val.to_string(), ""sv);
  check_eq(val.to_object().size(), 0u);
  check_eq(to_string(val), "null");
  check_eq(printed(val), "null");
  check_eq(deep_copy(val), val);
}

TEST("from undefined") {
  auto val = json_value::undefined();
  check(!val.is_null());
  check(val.is_undefined());
  check(!val.is_integer());
  check(!val.is_integer());
  check(!val.is_double());
  check(!val.is_number());
  check(!val.is_bool());
  check(!val.is_string());
  check(!val.is_array());
  check(!val.is_object());
  check_eq(val.to_integer(), 0);
  check_eq(val.to_unsigned(), 0u);
  check_eq(val.to_double(), test::approx{0.0});
  check_eq(val.to_bool(), false);
  check_eq(val.to_string(), ""sv);
  check_eq(val.to_object().size(), 0u);
  check_eq(to_string(val), "null");
  check_eq(printed(val), "null");
  check_eq(deep_copy(val), val);
}

TEST("from negative integer") {
  auto val = unbox(json_value::parse("-1"));
  check(!val.is_null());
  check(!val.is_undefined());
  check(val.is_integer());
  check(!val.is_unsigned());
  check(!val.is_double());
  check(val.is_number());
  check(!val.is_bool());
  check(!val.is_string());
  check(!val.is_array());
  check(!val.is_object());
  check_eq(val.to_integer(), -1);
  check_eq(val.to_unsigned(), 0u);
  check_eq(val.to_double(), test::approx{-1.0});
  check_eq(val.to_bool(), false);
  check_eq(val.to_string(), ""sv);
  check_eq(val.to_object().size(), 0u);
  check_eq(to_string(val), "-1");
  check_eq(printed(val), "-1");
  check_eq(deep_copy(val), val);
}

TEST("from small integer") {
  // A small integer can be represented as both int64_t and uint64_t.
  auto val = unbox(json_value::parse("42"));
  check(!val.is_null());
  check(!val.is_undefined());
  check(val.is_integer());
  check(val.is_unsigned());
  check(!val.is_double());
  check(val.is_number());
  check(!val.is_bool());
  check(!val.is_string());
  check(!val.is_array());
  check(!val.is_object());
  check_eq(val.to_integer(), 42);
  check_eq(val.to_unsigned(), 42u);
  check_eq(val.to_double(), test::approx{42.0});
  check_eq(val.to_bool(), false);
  check_eq(val.to_string(), ""sv);
  check_eq(val.to_object().size(), 0u);
  check_eq(to_string(val), "42");
  check_eq(printed(val), "42");
  check_eq(deep_copy(val), val);
}

TEST("from UINT64_MAX") {
  auto val = unbox(json_value::parse("18446744073709551615"));
  check(!val.is_null());
  check(!val.is_undefined());
  check(!val.is_integer());
  check(val.is_unsigned());
  check(!val.is_double());
  check(val.is_number());
  check(!val.is_bool());
  check(!val.is_string());
  check(!val.is_array());
  check(!val.is_object());
  check_eq(val.to_integer(), 0);
  check_eq(val.to_unsigned(), UINT64_MAX);
  check_eq(val.to_double(), test::approx{static_cast<double>(UINT64_MAX)});
  check_eq(val.to_bool(), false);
  check_eq(val.to_string(), ""sv);
  check_eq(val.to_object().size(), 0u);
  check_eq(to_string(val), "18446744073709551615");
  check_eq(printed(val), "18446744073709551615");
  check_eq(deep_copy(val), val);
}

TEST("from double") {
  auto val = unbox(json_value::parse("42.0"));
  check(!val.is_null());
  check(!val.is_undefined());
  check(!val.is_integer());
  check(val.is_double());
  check(val.is_number());
  check(!val.is_bool());
  check(!val.is_string());
  check(!val.is_array());
  check(!val.is_object());
  check_eq(val.to_integer(), 42);
  check_eq(val.to_unsigned(), 42u);
  check_eq(val.to_double(), test::approx{42.0});
  check_eq(val.to_bool(), false);
  check_eq(val.to_string(), ""sv);
  check_eq(val.to_object().size(), 0u);
  check_eq(to_string(val), "42");
  check_eq(printed(val), "42");
  check_eq(deep_copy(val), val);
}

TEST("from bool") {
  auto val = unbox(json_value::parse("true"));
  check(!val.is_null());
  check(!val.is_undefined());
  check(!val.is_integer());
  check(!val.is_double());
  check(!val.is_number());
  check(val.is_bool());
  check(!val.is_string());
  check(!val.is_array());
  check(!val.is_object());
  check_eq(val.to_integer(), 0);
  check_eq(val.to_unsigned(), 0u);
  check_eq(val.to_double(), test::approx{0.0});
  check_eq(val.to_bool(), true);
  check_eq(val.to_string(), ""sv);
  check_eq(val.to_object().size(), 0u);
  check_eq(to_string(val), "true");
  check_eq(printed(val), "true");
  check_eq(deep_copy(val), val);
}

TEST("from string") {
  auto val = unbox(json_value::parse(R"_("Hello, world!")_"));
  check(!val.is_null());
  check(!val.is_undefined());
  check(!val.is_integer());
  check(!val.is_double());
  check(!val.is_number());
  check(!val.is_bool());
  check(val.is_string());
  check(!val.is_array());
  check(!val.is_object());
  check_eq(val.to_integer(), 0);
  check_eq(val.to_unsigned(), 0u);
  check_eq(val.to_double(), test::approx{0.0});
  check_eq(val.to_bool(), false);
  check_eq(val.to_string(), "Hello, world!"sv);
  check_eq(val.to_object().size(), 0u);
  check_eq(to_string(val), R"_("Hello, world!")_");
  check_eq(printed(val), R"_("Hello, world!")_");
  check_eq(deep_copy(val), val);
}

TEST("from empty array") {
  auto val = unbox(json_value::parse("[]"));
  check(!val.is_null());
  check(!val.is_undefined());
  check(!val.is_integer());
  check(!val.is_double());
  check(!val.is_number());
  check(!val.is_bool());
  check(!val.is_string());
  check(val.is_array());
  check(!val.is_object());
  check_eq(val.to_integer(), 0);
  check_eq(val.to_unsigned(), 0u);
  check_eq(val.to_double(), test::approx{0.0});
  check_eq(val.to_bool(), false);
  check_eq(val.to_string(), ""sv);
  check_eq(val.to_array().size(), 0u);
  check_eq(val.to_object().size(), 0u);
  check_eq(to_string(val), "[]");
  check_eq(printed(val), "[]");
  check_eq(deep_copy(val), val);
}

TEST("from array of size 1") {
  auto val = unbox(json_value::parse("[1]"));
  check(!val.is_null());
  check(!val.is_undefined());
  check(!val.is_integer());
  check(!val.is_double());
  check(!val.is_number());
  check(!val.is_bool());
  check(!val.is_string());
  check(val.is_array());
  check(!val.is_object());
  check_eq(val.to_integer(), 0);
  check_eq(val.to_unsigned(), 0u);
  check_eq(val.to_double(), test::approx{0.0});
  check_eq(val.to_bool(), false);
  check_eq(val.to_string(), ""sv);
  check_eq(val.to_array().size(), 1u);
  check_eq(val.to_object().size(), 0u);
  check_eq(to_string(val), "[1]");
  check_eq(printed(val), "[\n  1\n]");
  check_eq(deep_copy(val), val);
}

TEST("from array of size 3") {
  auto val = unbox(json_value::parse("[1, 2, 3]"));
  check(!val.is_null());
  check(!val.is_undefined());
  check(!val.is_integer());
  check(!val.is_double());
  check(!val.is_number());
  check(!val.is_bool());
  check(!val.is_string());
  check(val.is_array());
  check(!val.is_object());
  check_eq(val.to_integer(), 0);
  check_eq(val.to_unsigned(), 0u);
  check_eq(val.to_double(), test::approx{0.0});
  check_eq(val.to_bool(), false);
  check_eq(val.to_string(), ""sv);
  check_eq(val.to_array().size(), 3u);
  check_eq(val.to_object().size(), 0u);
  check_eq(to_string(val), "[1, 2, 3]");
  check_eq(printed(val), "[\n  1,\n  2,\n  3\n]");
  check_eq(deep_copy(val), val);
}

TEST("from empty object") {
  auto val = unbox(json_value::parse("{}"));
  check(!val.is_null());
  check(!val.is_undefined());
  check(!val.is_integer());
  check(!val.is_double());
  check(!val.is_number());
  check(!val.is_bool());
  check(!val.is_string());
  check(!val.is_array());
  check(val.is_object());
  check_eq(val.to_integer(), 0);
  check_eq(val.to_unsigned(), 0u);
  check_eq(val.to_double(), test::approx{0.0});
  check_eq(val.to_bool(), false);
  check_eq(val.to_string(), ""sv);
  check_eq(val.to_object().size(), 0u);
  check_eq(to_string(val), "{}");
  check_eq(printed(val), "{}");
  check_eq(deep_copy(val), val);
}

TEST("from non-empty object") {
  auto val = unbox(json_value::parse(R"_({"foo": "bar"})_"));
  check(!val.is_null());
  check(!val.is_undefined());
  check(!val.is_integer());
  check(!val.is_double());
  check(!val.is_number());
  check(!val.is_bool());
  check(!val.is_string());
  check(!val.is_array());
  check(val.is_object());
  check_eq(val.to_integer(), 0);
  check_eq(val.to_unsigned(), 0u);
  check_eq(val.to_double(), test::approx{0.0});
  check_eq(val.to_bool(), false);
  check_eq(val.to_string(), ""sv);
  check_eq(val.to_object().size(), 1u);
  check_eq(to_string(val), R"_({"foo": "bar"})_");
  check_eq(printed(val), "{\n  \"foo\": \"bar\"\n}");
  check_eq(deep_copy(val), val);
}

std::string_view json_with_boundary_values = R"_({
  "min-int64": -9223372036854775808,
  "max-int64": 9223372036854775807,
  "min-uint64": 0,
  "max-uint64": 18446744073709551615
})_";

TEST("non-empty object with nested values") {
  auto val = unbox(json_value::parse(json_with_boundary_values));
  check(val.is_object());
  auto obj = val.to_object();
  check_eq(obj.value("min-int64").to_integer(), INT64_MIN);
  check_eq(obj.value("max-int64").to_integer(), INT64_MAX);
  check_eq(obj.value("min-uint64").to_unsigned(), 0u);
  check_eq(obj.value("max-uint64").to_unsigned(), UINT64_MAX);
}

