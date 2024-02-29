// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/json_object.hpp"

#include "caf/test/test.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/json_array.hpp"
#include "caf/json_value.hpp"

using namespace caf;
using namespace std::literals;

namespace {

template <class T>
T deep_copy(const T& val) {
  using namespace std::literals;
  caf::byte_buffer buf;
  {
    caf::binary_serializer sink{nullptr, buf};
    if (!sink.apply(val)) {
      auto msg = "serialization failed in deep_copy: "s;
      msg += to_string(sink.get_error());
      CAF_RAISE_ERROR(msg.c_str());
    }
  }
  auto result = T{};
  {
    caf::binary_deserializer sink{nullptr, buf};
    if (!sink.apply(result)) {
      auto msg = "deserialization failed in deep_copy: "s;
      msg += to_string(sink.get_error());
      CAF_RAISE_ERROR(msg.c_str());
    }
  }
  return result;
}

std::string printed(const json_object& obj) {
  std::string result;
  obj.print_to(result, 2);
  return result;
}

} // namespace

TEST("default-constructed") {
  auto obj = json_object{};
  check(obj.empty());
  check(obj.is_empty());
  check(obj.begin() == obj.end());
  check_eq(obj.size(), 0u);
  check(obj.value("foo").is_undefined());
  check_eq(to_string(obj), "{}");
  check_eq(printed(obj), "{}");
  check_eq(deep_copy(obj), obj);
}

TEST("from empty object") {
  auto obj = json_value::parse("{}")->to_object();
  check(obj.empty());
  check(obj.is_empty());
  check(obj.begin() == obj.end());
  check_eq(obj.size(), 0u);
  check(obj.value("foo").is_undefined());
  check_eq(to_string(obj), "{}");
  check_eq(printed(obj), "{}");
  check_eq(deep_copy(obj), obj);
}

TEST("from non-empty object") {
  auto obj = json_value::parse(R"_({"a": "one", "b": 2})_")->to_object();
  check(!obj.empty());
  check(!obj.is_empty());
  check(obj.begin() != obj.end());
  require_eq(obj.size(), 2u);
  check_eq(obj.begin().key(), "a");
  check_eq(obj.begin().value().to_string(), "one");
  check_eq(obj.value("a").to_string(), "one");
  check_eq(obj.value("b").to_integer(), 2);
  check(obj.value("c").is_undefined());
  std::vector<std::pair<std::string_view, json_value>> vals;
  for (const auto& val : obj) {
    vals.emplace_back(val);
  }
  require_eq(vals.size(), 2u);
  check_eq(vals[0].first, "a");
  check_eq(vals[0].second.to_string(), "one");
  check_eq(vals[1].first, "b");
  check_eq(vals[1].second.to_integer(), 2);
  check_eq(to_string(obj), R"_({"a": "one", "b": 2})_");
  check_eq(printed(obj), "{\n  \"a\": \"one\",\n  \"b\": 2\n}");
  check_eq(deep_copy(obj), obj);
}
