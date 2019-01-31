/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <string>
#include <vector>

#include "caf/config.hpp"

#define CAF_SUITE read_ini
#include "caf/test/dsl.hpp"

#include "caf/detail/parser/read_ini.hpp"

using namespace caf;

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

struct ini_consumer {
  using inner_map = std::map<std::string, config_value>;
  using section_map = std::map<std::string, inner_map>;

  section_map sections;
  section_map::iterator current_section;
};

struct fixture {
  expected<log_type> parse(std::string str, bool expect_success = true) {
    detail::parser::state<std::string::iterator> res;
    test_consumer f;
    res.i = str.begin();
    res.e = str.end();
    detail::parser::read_ini(res, f);
    if ((res.code == pec::success) != expect_success) {
      CAF_MESSAGE("unexpected parser result state: " << res.code);
      CAF_MESSAGE("input remainder: " << std::string(res.i, res.e));
    }
    return std::move(f.log);
  }
};

template <class... Ts>
log_type make_log(Ts&&... xs) {
  return log_type{std::forward<Ts>(xs)...};
}

const char* ini0 = R"(
[logger]
padding= 10
file-name = "foobar.ini" ; our file name

[scheduler] ; more settings
  timing  =  2us ; using microsecond resolution
impl =       'foo';some atom
x_ =.123
some-bool=true
some-other-bool=false
some-list=[
; here we have some list entries
123,
  23 ; twenty-three!
  ,
  "abc",
  'def', ; some comment and a trailing comma
]
some-map{
; here we have some list entries
entry1=123,
  entry2=23 ; twenty-three! btw, comma is not mandatory
 entry3= "abc",
 entry4 = 'def', ; some comment and a trailing comma
}
[middleman]
preconnect=[<
tcp://localhost:8080

   >,<udp://remotehost?trust=false>]
)";

const auto ini0_log = make_log(
  "key: logger",
  "{",
    "key: padding",
    "value (integer): 10",
    "key: file-name",
    "value (string): \"foobar.ini\"",
  "}",
  "key: scheduler",
  "{",
    "key: timing",
    "value (timespan): 2us",
    "key: impl",
    "value (atom): 'foo'",
    "key: x_",
    "value (real): " + deep_to_string(.123),
    "key: some-bool",
    "value (boolean): true",
    "key: some-other-bool",
    "value (boolean): false",
    "key: some-list",
    "[",
      "value (integer): 123",
      "value (integer): 23",
      "value (string): \"abc\"",
      "value (atom): 'def'",
    "]",
    "key: some-map",
    "{",
      "key: entry1",
      "value (integer): 123",
      "key: entry2",
      "value (integer): 23",
      "key: entry3",
      "value (string): \"abc\"",
      "key: entry4",
      "value (atom): 'def'",
    "}",
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

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(read_ini_tests, fixture)

CAF_TEST(empty inis) {
  CAF_CHECK_EQUAL(parse(";foo"), make_log());
  CAF_CHECK_EQUAL(parse(""), make_log());
  CAF_CHECK_EQUAL(parse("  "), make_log());
  CAF_CHECK_EQUAL(parse(" \n "), make_log());
  CAF_CHECK_EQUAL(parse(";hello\n;world"), make_log());
}

CAF_TEST(section with valid key-value pairs) {
  CAF_CHECK_EQUAL(parse("[foo]"), make_log("key: foo", "{", "}"));
  CAF_CHECK_EQUAL(parse("  [foo]"), make_log("key: foo", "{", "}"));
  CAF_CHECK_EQUAL(parse("  [  foo]  "), make_log("key: foo", "{", "}"));
  CAF_CHECK_EQUAL(parse("  [  foo  ]  "), make_log("key: foo", "{", "}"));
  CAF_CHECK_EQUAL(parse("\n[a-b];foo\n;bar"), make_log("key: a-b", "{", "}"));
  CAF_CHECK_EQUAL(parse(ini0), ini0_log);
}

CAF_TEST_FIXTURE_SCOPE_END()
