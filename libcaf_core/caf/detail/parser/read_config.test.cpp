// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/parser/read_config.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/config_value.hpp"
#include "caf/log/test.hpp"
#include "caf/parser_state.hpp"
#include "caf/pec.hpp"

#include <string_view>

using namespace caf;
using namespace std::literals;

namespace {

using log_type = std::vector<std::string>;

struct test_consumer {
  log_type log;

  test_consumer() = default;
  test_consumer(const test_consumer&) = delete;
  test_consumer& operator=(const test_consumer&) = delete;

  test_consumer& begin_map() {
    log.emplace_back("{");
    return *this;
  }

  void end_map() {
    log.emplace_back("}");
  }

  test_consumer& begin_list() {
    log.emplace_back("[");
    return *this;
  }

  void end_list() {
    log.emplace_back("]");
  }

  void key(std::string name) {
    add_entry("key: ", std::move(name));
  }

  template <class T>
  void value(T x) {
    config_value cv{std::move(x)};
    std::string entry = "value (";
    entry += cv.type_name();
    entry += "): ";
    entry += to_string(cv);
    log.emplace_back(std::move(entry));
  }

  void add_entry(const char* prefix, std::string str) {
    str.insert(0, prefix);
    log.emplace_back(std::move(str));
  }
};

struct fixture : test::fixture::deterministic {
  expected<log_type> parse(std::string_view str, bool expect_success = true) {
    test_consumer f;
    string_parser_state res{str.begin(), str.end()};
    detail::parser::read_config(res, f);
    if ((res.code == pec::success) != expect_success) {
      log::test::error("unexpected parser result state: {}", res.code);
      log::test::error("input remainder: {}", std::string{res.i, res.e});
    }
    return std::move(f.log);
  }
};

template <class... Ts>
log_type make_log(Ts&&... xs) {
  return log_type{std::forward<Ts>(xs)...};
}

// Tests basic functionality.
constexpr const std::string_view conf0 = R"(
"foo=bar" {
  foo="bar"
}
1group {
  1value=321
}
_foo {
  _bar=11
}
logger {
  "padding":10
  file-name = "foobar.ini" # our file name
}

scheduler { # more settings
    timing  =  2us # using microsecond resolution
  x_ =.123
  some-bool=true
  some-other-bool=false
}
some-list=[
# here we have some list entries
123,
  1..3,
  23 # twenty-three!
  ,2..4..2,
  "abc", # some comment and a trailing comma
]
some-map:{
# here we have some list entries
entry1:123,
  entry2=23 # twenty-three! btw, comma is not mandatory
 entry3= "abc" , # some comment and a trailing comma
}
middleman {
  preconnect=[<
  tcp://localhost:8080

     >,<udp://remotehost?trust=false>]
}
)";

// clang-format off
const auto conf0_log = make_log(
  "key: foo=bar",
  "{",
    "key: foo",
    "value (string): bar",
  "}",
  "key: 1group",
  "{",
    "key: 1value",
    "value (integer): 321",
  "}",
  "key: _foo",
  "{",
    "key: _bar",
    "value (integer): 11",
  "}",
  "key: logger",
  "{",
    "key: padding",
    "value (integer): 10",
    "key: file-name",
    "value (string): foobar.ini",
  "}",
  "key: scheduler",
  "{",
    "key: timing",
    "value (timespan): 2us",
    "key: x_",
    "value (real): " + deep_to_string(.123),
    "key: some-bool",
    "value (boolean): true",
    "key: some-other-bool",
    "value (boolean): false",
  "}",
  "key: some-list",
  "[",
    "value (integer): 123",
    "value (integer): 1",
    "value (integer): 2",
    "value (integer): 3",
    "value (integer): 23",
    "value (integer): 2",
    "value (integer): 4",
    "value (string): abc",
  "]",
  "key: some-map",
  "{",
    "key: entry1",
    "value (integer): 123",
    "key: entry2",
    "value (integer): 23",
    "key: entry3",
    "value (string): abc",
  "}",
  "key: middleman",
  "{",
    "key: preconnect",
    "[",
      "value (uri): tcp://localhost:8080",
      "value (uri): udp://remotehost?trust=false",
    "]",
  "}"
);

constexpr const std::string_view conf1 = R"(
{
    "foo" : {
        "bar" : 1,
        "baz" : 2
    }
}
)";

const auto conf1_log = make_log(
  "key: foo",
  "{",
    "key: bar",
    "value (integer): 1",
    "key: baz",
    "value (integer): 2",
  "}"
);
// clang-format on

std::string with_windows_newlines(std::string_view str) {
  std::string result;
  for (auto c : str) {
    if (c == '\n') {
      result += "\r\n";
    } else {
      result += c;
    }
  }
  return result;
}

WITH_FIXTURE(fixture) {

TEST("read_config feeds into a consumer") {
  check_eq(parse(conf0), conf0_log);
  check_eq(parse(conf1), conf1_log);
  check_eq(parse(with_windows_newlines(conf0)), conf0_log);
  check_eq(parse(with_windows_newlines(conf1)), conf1_log);
}

} // WITH_FIXTURE(fixture)

} // namespace
