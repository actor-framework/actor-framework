// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/json_array.hpp"

#include "caf/test/test.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/json_value.hpp"

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

std::string printed(const json_array& arr) {
  std::string result;
  arr.print_to(result, 2);
  return result;
}

TEST("default-constructed") {
  auto arr = json_array{};
  check(arr.empty());
  check(arr.is_empty());
  check(arr.begin() == arr.end());
  check_eq(arr.size(), 0u);
  check_eq(to_string(arr), "[]");
  check_eq(printed(arr), "[]");
  check_eq(deep_copy(arr), arr);
}

TEST("from empty array") {
  auto arr = json_value::parse("[]")->to_array();
  check(arr.empty());
  check(arr.is_empty());
  check(arr.begin() == arr.end());
  check_eq(arr.size(), 0u);
  check_eq(to_string(arr), "[]");
  check_eq(printed(arr), "[]");
  check_eq(deep_copy(arr), arr);
}

TEST("from non-empty array") {
  auto arr = json_value::parse(R"_([1, "two", 3.0])_")->to_array();
  check(!arr.empty());
  check(!arr.is_empty());
  check(arr.begin() != arr.end());
  require_eq(arr.size(), 3u);
  check_eq((*arr.begin()).to_integer(), 1);
  std::vector<json_value> vals;
  for (const auto& val : arr) {
    vals.emplace_back(val);
  }
  require_eq(vals.size(), 3u);
  check_eq(vals[0].to_integer(), 1);
  check_eq(vals[1].to_string(), "two");
  check_eq(vals[2].to_double(), 3.0);
  check_eq(to_string(arr), R"_([1, "two", 3])_");
  check_eq(printed(arr), "[\n  1,\n  \"two\",\n  3\n]");
  check_eq(deep_copy(arr), arr);
}

} // namespace
