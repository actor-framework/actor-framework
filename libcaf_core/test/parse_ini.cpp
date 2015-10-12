/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

#define CAF_SUITE parse_ini
#include "caf/test/unit_test.hpp"

#include <iostream>
#include <sstream>

#include "caf/all.hpp"

#include "caf/experimental/whereis.hpp"

#include "caf/detail/parse_ini.hpp"
#include "caf/detail/safe_equal.hpp"

using namespace caf;

namespace {

constexpr const char* case1 = R"__(
[scheduler]
policy="work-sharing"
max-threads=2
; the middleman
[middleman]
automatic-connections=true

[nexus]
host="127.0.0.1"
port=4242

[cash]
greeting="Hi there, this is \"CASH!\"\n ~\\~ use at your own risk ~\\~"
)__";

constexpr const char* case2 = R"__(
[test]
foo=-0xff
bar=034
baz=-0.23
buzz=1E-34
bazz=0b10101010110011
)__";

constexpr const char* case3 = R"__("
[whoops
foo="bar"
[test]
; provoke some more errors
foo bar
=42
baz=
foo="
bar="foo
some-int=42
some-string="hi there!\"
neg=-
wtf=0x3733T
not-a-bin=0b101002
hu=0779
hop=--"hiho"
)__";

struct fixture {
  ~fixture() {
    shutdown();
  }

  template <class F>
  void load_impl(F loader, const char* str) {
    std::stringstream ss;
    std::stringstream err;
    ss << str;
    loader(ss, err);
    split(errors, err.str(), is_any_of("\n"), token_compress_on);
  }

  void load_to_config_server(const char* str) {
    config_server = experimental::whereis(atom("ConfigServ"));;
    auto f = [&](std::istream& in, std::ostream& out) {
      parse_config(in, config_format::ini, out);
    };
    load_impl(f, str);
  }

  void load(const char* str) {
    auto consume = [&](std::string key, config_value value) {
      values.emplace(std::move(key), std::move(value));
    };
    auto f = [&](std::istream& in, std::ostream& out) {
      detail::parse_ini(in, consume, out);
    };
    load_impl(f, str);
  }

  bool has_error(const char* err) {
    return std::any_of(errors.begin(), errors.end(),
                       [=](const std::string& str) { return str == err; });
  }

  template <class T>
  bool config_server_has(const char* key, const T& what) {
    using type =
      typename std::conditional<
        std::is_convertible<T, std::string>::value,
        std::string,
        typename std::conditional<
          std::is_integral<T>::value && ! std::is_same<T, bool>::value,
          int64_t,
          T
        >::type
      >::type;
    bool result = false;
    scoped_actor self;
    self->sync_send(config_server, get_atom::value, key).await(
      [&](ok_atom, std::string&, message& msg) {
        msg.apply(
          [&](type& val) {
            result = detail::safe_equal(what, val);
          }
        );
      }
    );
    return result;
  }

  template <class T>
  bool value_is(const char* key, const T& what) {
    if (config_server != invalid_actor)
      return config_server_has(key, what);
    auto& cv = values[key];
    using type =
      typename std::conditional<
        std::is_convertible<T, std::string>::value,
        std::string,
        typename std::conditional<
          std::is_integral<T>::value && ! std::is_same<T, bool>::value,
          int64_t,
          T
        >::type
      >::type;
    auto ptr = get<type>(&cv);
    return ptr != nullptr && detail::safe_equal(*ptr, what);
  }

  size_t num_values() {
    if (config_server != invalid_actor) {
      size_t result = 0;
      scoped_actor self;
      self->sync_send(config_server, get_atom::value, "*").await(
        [&](ok_atom, std::vector<std::pair<std::string, message>>& msgs) {
          result = msgs.size();
        }
      );
      return result;
    }
    return values.size();
  }

  void check_case1() {
    CAF_CHECK(errors.empty());
    CAF_CHECK(num_values() == 6);
    CAF_CHECK(value_is("nexus.port", 4242));
    CAF_CHECK(value_is("nexus.host", "127.0.0.1"));
    CAF_CHECK(value_is("scheduler.policy", "work-sharing"));
    CAF_CHECK(value_is("scheduler.max-threads", 2));
    CAF_CHECK(value_is("middleman.automatic-connections", true));
    CAF_CHECK(value_is("cash.greeting",
              "Hi there, this is \"CASH!\"\n ~\\~ use at your own risk ~\\~"));
  }

  void check_case2() {
    CAF_CHECK(errors.empty());
    CAF_CHECK(num_values() == 5);
    CAF_CHECK(value_is("test.foo", -0xff));
    CAF_CHECK(value_is("test.bar", 034));
    CAF_CHECK(value_is("test.baz", -0.23));
    CAF_CHECK(value_is("test.buzz", 1E-34));
    CAF_CHECK(value_is("test.bazz", 10931));
  }

  void check_case3() {
    CAF_CHECK(has_error("error in line 2: missing ] at end of line"));
    CAF_CHECK(has_error("error in line 3: value outside of a group"));
    CAF_CHECK(has_error("error in line 6: no '=' found"));
    CAF_CHECK(has_error("error in line 7: line starting with '='"));
    CAF_CHECK(has_error("error in line 8: line ends with '='"));
    CAF_CHECK(has_error("error in line 9: stray '\"'"));
    CAF_CHECK(has_error("error in line 10: string not terminated by '\"'"));
    CAF_CHECK(has_error("warning in line 12: trailing quotation mark escaped"));
    CAF_CHECK(has_error("error in line 13: '-' is not a number"));
    CAF_CHECK(has_error("error in line 14: invalid hex value"));
    CAF_CHECK(has_error("error in line 15: invalid binary value"));
    CAF_CHECK(has_error("error in line 16: invalid oct value"));
    CAF_CHECK(has_error("error in line 17: invalid value"));
    CAF_CHECK(num_values() == 2);
    CAF_CHECK(value_is("test.some-int", 42));
    CAF_CHECK(value_is("test.some-string", "hi there!"));
  }

  std::map<std::string, config_value> values;
  actor config_server;
  std::vector<std::string> errors;
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(parse_ini_tests, fixture)



CAF_TEST(simple_ini) {
  load(case1);
  check_case1();

}

CAF_TEST(numbers) {
  load(case2);
  check_case2();
}

CAF_TEST(errors) {
  load_to_config_server(case3);
  check_case3();
}

CAF_TEST(simple_ini_via_config_server) {
  load_to_config_server(case1);
  CAF_CHECK(values.empty());
  check_case1();
}

CAF_TEST(numbers_via_config_server) {
  load_to_config_server(case2);
  CAF_CHECK(values.empty());
  check_case2();
}

CAF_TEST(errors_via_config_server) {
  load_to_config_server(case3);
  CAF_CHECK(values.empty());
  check_case3();
}

CAF_TEST_FIXTURE_SCOPE_END()
