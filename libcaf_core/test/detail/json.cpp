// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.json

#include "caf/detail/json.hpp"

#include "core-test.hpp"

using namespace caf;
using namespace caf::literals;

namespace {

// Worth mentioning: the output we check against is the trivial format produced
// by stringify(), which is not valid JSON due to trailing commas.
constexpr std::pair<string_view, string_view> baselines[] = {
  {
    R"({})",
    R"({})",
  },
  {
    R"(  {      } )",
    R"({})",
  },
  {
    R"(42)",
    R"(42)",
  },
  {
    R"(true)",
    R"(true)",
  },
  {
    R"(false)",
    R"(false)",
  },
  {
    R"(null)",
    R"(null)",
  },
  {
    R"({"foo":"bar"})",
    R"({
  "foo": "bar",
})",
  },
  {
    R"(["foo","bar"])",
    R"([
  "foo",
  "bar",
])",
  },
  {
    R"({
  "ints":[1,2,3],"awesome?":true,"ptr":null,"empty-list":[],"nested":{
    "hello": "world",
    "greeting": "hello world!"
  },
  "empty-object": {}
})",
    R"({
  "ints": [
    1,
    2,
    3,
  ],
  "awesome?": true,
  "ptr": null,
  "empty-list": [],
  "nested": {
    "hello": "world",
    "greeting": "hello world!",
  },
  "empty-object": {},
})",
  },
};

void stringify(std::string& str, size_t indent, const detail::json::value& val);

void stringify(std::string& str, size_t, int64_t val) {
  str += std::to_string(val);
}

void stringify(std::string& str, size_t, double val) {
  str += std::to_string(val);
}

void stringify(std::string& str, size_t, bool val) {
  if (val)
    str += "true";
  else
    str += "false";
}

void stringify(std::string& str, size_t, string_view val) {
  str.push_back('"');
  str.insert(str.end(), val.begin(), val.end());
  str.push_back('"');
}

void stringify(std::string& str, size_t, detail::json::null_t) {
  str += "null";
}

void stringify(std::string& str, size_t indent, const detail::json::array& xs) {
  if (xs.empty()) {
    str += "[]";
  } else {
    str += "[\n";
    for (const auto& x : xs) {
      str.insert(str.end(), indent + 2, ' ');
      stringify(str, indent + 2, x);
      str += ",\n";
    }
    str.insert(str.end(), indent, ' ');
    str += "]";
  }
}

void stringify(std::string& str, size_t indent,
               const detail::json::object& obj) {
  if (obj.empty()) {
    str += "{}";
  } else {
    str += "{\n";
    for (const auto& [key, val] : obj) {
      str.insert(str.end(), indent + 2, ' ');
      stringify(str, indent + 2, key);
      str += ": ";
      stringify(str, indent + 2, *val);
      str += ",\n";
    }
    str.insert(str.end(), indent, ' ');
    str += "}";
  }
}

void stringify(std::string& str, size_t indent,
               const detail::json::value& val) {
  auto f = [&str, indent](auto&& x) { stringify(str, indent, x); };
  std::visit(f, val.data);
}

std::string stringify(const detail::json::value& val) {
  std::string result;
  stringify(result, 0, val);
  return result;
}

} // namespace

CAF_TEST(json baselines) {
  size_t baseline_index = 0;
  detail::monotonic_buffer_resource resource;
  for (auto [input, output] : baselines) {
    MESSAGE("test baseline at index " << baseline_index++);
    string_parser_state ps{input.begin(), input.end()};
    auto val = detail::json::parse(ps, &resource);
    CHECK_EQ(ps.code, pec::success);
    CHECK_EQ(stringify(*val), output);
    resource.reclaim();
  }
}
