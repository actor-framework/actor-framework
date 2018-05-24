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

  void begin_map() {
    log.emplace_back("{");
  }

  void end_map() {
    log.emplace_back("}");
  }

  void begin_list() {
    log.emplace_back("[");
  }

  void end_list() {
    log.emplace_back("]");
  }

  void key(std::string name) {
    add_entry("key: ", std::move(name));
  }

  template <class T>
  void value(T x) {
    add_entry("value: ", deep_to_string(x));
  }

  void add_entry(const char* prefix, std::string str) {
    str.insert(0, prefix);
    log.emplace_back(std::move(str));
  }
};

struct fixture {
  expected<log_type> parse(std::string str, bool expect_success = true) {
    detail::parser::state<std::string::iterator> res;
    test_consumer f;
    res.i = str.begin();
    res.e = str.end();
    detail::parser::read_ini(res, f);
    if (res.code == detail::parser::ec::success != expect_success)
      CAF_MESSAGE("unexpected parser result state: " << res.code);
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
)";

const auto ini0_log = make_log(
  "key: logger", "{", "key: padding", "value: 10", "key: file-name",
  "value: \"foobar.ini\"", "}", "key: scheduler", "{", "key: timing",
  "value: 2000ns", "key: impl", "value: 'foo'", "key: x_",
  "value: " + deep_to_string(.123), "key: some-bool", "value: true",
  "key: some-other-bool", "value: false", "key: some-list", "[", "value: 123",
  "value: 23", "value: \"abc\"", "value: 'def'", "]", "}");

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
