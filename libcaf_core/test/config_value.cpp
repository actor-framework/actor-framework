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

#include "caf/config.hpp"

#define CAF_SUITE config_value
#include "caf/test/unit_test.hpp"

#include <string>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/atom.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/detail/bounds_checker.hpp"
#include "caf/none.hpp"
#include "caf/pec.hpp"
#include "caf/variant.hpp"
#include "caf/string_view.hpp"

using std::string;

using namespace caf;

namespace {

using list = config_value::list;

using dictionary = config_value::dictionary;

struct dictionary_builder {
  dictionary dict;

  dictionary_builder&& add(string_view key, config_value value) && {
    dict.emplace(key, std::move(value));
    return std::move(*this);
  }

  dictionary make() && {
    return std::move(dict);
  }

  config_value make_cv() && {
    return config_value{std::move(dict)};
  }
};

dictionary_builder dict() {
  return {};
}

template <class... Ts>
config_value cfg_lst(Ts&&... xs) {
  config_value::list lst{config_value{std::forward<Ts>(xs)}...};
  return config_value{std::move(lst)};
}

// TODO: switch to std::operator""s when switching to C++14
string operator"" _s(const char* str, size_t size) {
  return string(str, size);
}

} // namespace <anonymous>

CAF_TEST(default_constructed) {
  config_value x;
  CAF_CHECK_EQUAL(holds_alternative<int64_t>(x), true);
  CAF_CHECK_EQUAL(get<int64_t>(x), 0);
  CAF_CHECK_EQUAL(x.type_name(), "integer"_s);
}

CAF_TEST(positive integer) {
  config_value x{4200};
  CAF_CHECK_EQUAL(holds_alternative<int64_t>(x), true);
  CAF_CHECK_EQUAL(get<int64_t>(x), 4200);
  CAF_CHECK(get_if<int64_t>(&x) != nullptr);
  CAF_CHECK_EQUAL(holds_alternative<uint64_t>(x), true);
  CAF_CHECK_EQUAL(get<uint64_t>(x), 4200u);
  CAF_CHECK_EQUAL(get_if<uint64_t>(&x), uint64_t{4200});
  CAF_CHECK_EQUAL(holds_alternative<int>(x), true);
  CAF_CHECK_EQUAL(get<int>(x), 4200);
  CAF_CHECK_EQUAL(get_if<int>(&x), 4200);
  CAF_CHECK_EQUAL(holds_alternative<int16_t>(x), true);
  CAF_CHECK_EQUAL(get<int16_t>(x), 4200);
  CAF_CHECK_EQUAL(get_if<int16_t>(&x), int16_t{4200});
  CAF_CHECK_EQUAL(holds_alternative<int8_t>(x), false);
  CAF_CHECK_EQUAL(get_if<int8_t>(&x), caf::none);
}

CAF_TEST(negative integer) {
  config_value x{-1};
  CAF_CHECK_EQUAL(holds_alternative<int64_t>(x), true);
  CAF_CHECK_EQUAL(get<int64_t>(x), -1);
  CAF_CHECK(get_if<int64_t>(&x) != nullptr);
  CAF_CHECK_EQUAL(holds_alternative<uint64_t>(x), false);
  CAF_CHECK_EQUAL(get_if<uint64_t>(&x), none);
  CAF_CHECK_EQUAL(holds_alternative<int>(x), true);
  CAF_CHECK_EQUAL(get<int>(x), -1);
  CAF_CHECK_EQUAL(get_if<int>(&x), -1);
  CAF_CHECK_EQUAL(holds_alternative<int16_t>(x), true);
  CAF_CHECK_EQUAL(get<int16_t>(x), -1);
  CAF_CHECK_EQUAL(get_if<int16_t>(&x), int16_t{-1});
  CAF_CHECK_EQUAL(holds_alternative<int8_t>(x), true);
  CAF_CHECK_EQUAL(get_if<int8_t>(&x), int8_t{-1});
  CAF_CHECK_EQUAL(holds_alternative<uint8_t>(x), false);
  CAF_CHECK_EQUAL(get_if<uint8_t>(&x), none);
}

CAF_TEST(timespan) {
  timespan ns500{500};
  config_value x{ns500};
  CAF_CHECK_EQUAL(holds_alternative<timespan>(x), true);
  CAF_CHECK_EQUAL(get<timespan>(x), ns500);
  CAF_CHECK_NOT_EQUAL(get_if<timespan>(&x), nullptr);
}

CAF_TEST(list) {
  using integer_list = std::vector<int64_t>;
  auto xs = make_config_value_list(1, 2, 3);
  CAF_CHECK_EQUAL(to_string(xs), "[1, 2, 3]");
  CAF_CHECK_EQUAL(xs.type_name(), "list"_s);
  CAF_CHECK_EQUAL(holds_alternative<config_value::list>(xs), true);
  CAF_CHECK_EQUAL(holds_alternative<integer_list>(xs), true);
  CAF_CHECK_EQUAL(get<integer_list>(xs), integer_list({1, 2, 3}));
}

CAF_TEST(convert_to_list) {
  config_value x{int64_t{42}};
  CAF_CHECK_EQUAL(x.type_name(), "integer"_s);
  CAF_CHECK_EQUAL(to_string(x), "42");
  x.convert_to_list();
  CAF_CHECK_EQUAL(x.type_name(), "list"_s);
  CAF_CHECK_EQUAL(to_string(x), "[42]");
  x.convert_to_list();
  CAF_CHECK_EQUAL(to_string(x), "[42]");
}

CAF_TEST(append) {
  config_value x{int64_t{1}};
  CAF_CHECK_EQUAL(to_string(x), "1");
  x.append(config_value{int64_t{2}});
  CAF_CHECK_EQUAL(to_string(x), "[1, 2]");
  x.append(config_value{atom("foo")});
  CAF_CHECK_EQUAL(to_string(x), "[1, 2, 'foo']");
}

CAF_TEST(homogeneous dictionary) {
  using integer_map = caf::dictionary<int64_t>;
  auto xs = dict()
              .add("value-1", config_value{100000})
              .add("value-2", config_value{2})
              .add("value-3", config_value{3})
              .add("value-4", config_value{4})
              .make();
  integer_map ys{
    {"value-1", 100000},
    {"value-2", 2},
    {"value-3", 3},
    {"value-4", 4},
  };
  config_value xs_cv{xs};
  CAF_CHECK_EQUAL(get_if<int64_t>(&xs, "value-1"), int64_t{100000});
  CAF_CHECK_EQUAL(get_if<int32_t>(&xs, "value-1"), int32_t{100000});
  CAF_CHECK_EQUAL(get_if<int16_t>(&xs, "value-1"), none);
  CAF_CHECK_EQUAL(get<int64_t>(xs, "value-1"), 100000);
  CAF_CHECK_EQUAL(get<int32_t>(xs, "value-1"), 100000);
  CAF_CHECK_EQUAL(get_if<integer_map>(&xs_cv), ys);
  CAF_CHECK_EQUAL(get<integer_map>(xs_cv), ys);
}

CAF_TEST(heterogeneous dictionary) {
  using string_list = std::vector<string>;
  auto xs = dict()
              .add("scheduler", dict()
                                  .add("policy", config_value{atom("none")})
                                  .add("max-threads", config_value{2})
                                  .make_cv())
              .add("nodes", dict()
                              .add("preload", cfg_lst("sun", "venus", "mercury",
                                                      "earth", "mars"))
                              .make_cv())

              .make();
  CAF_CHECK_EQUAL(get<atom_value>(xs, "scheduler.policy"), atom("none"));
  CAF_CHECK_EQUAL(get<int64_t>(xs, "scheduler.max-threads"), 2);
  CAF_CHECK_EQUAL(get_if<double>(&xs, "scheduler.max-threads"), none);
  string_list nodes{"sun", "venus", "mercury", "earth", "mars"};
  CAF_CHECK_EQUAL(get<string_list>(xs, "nodes.preload"), nodes);
}

CAF_TEST(successful parsing) {
  // Store the parsed value on the stack, because the unit test framework takes
  // references when comparing values. Since we call get<T>() on the result of
  // parse(), we would end up with a reference to a temporary.
  config_value parsed;
  auto parse = [&](const string& str) -> config_value& {
    auto x = config_value::parse(str);
    if (!x)
      CAF_FAIL("cannot parse " << str << ": assumed a result but error "
               << to_string(x.error()));
    parsed = std::move(*x);
    return parsed;
  };
  using di = caf::dictionary<int>; // Dictionary-of-integers.
  using ls = std::vector<string>; // List-of-strings.
  using li = std::vector<int>; // List-of-integers.
  using lli = std::vector<li>; // List-of-list-of-integers.
  using std::chrono::milliseconds;
  CAF_CHECK_EQUAL(get<int64_t>(parse("123")), 123);
  CAF_CHECK_EQUAL(get<int64_t>(parse("+123")), 123);
  CAF_CHECK_EQUAL(get<int64_t>(parse("-1")), -1);
  CAF_CHECK_EQUAL(get<double>(parse("1.")), 1.);
  CAF_CHECK_EQUAL(get<atom_value>(parse("'abc'")), atom("abc"));
  CAF_CHECK_EQUAL(get<string>(parse("\"abc\"")), "abc");
  CAF_CHECK_EQUAL(get<string>(parse("abc")), "abc");
  CAF_CHECK_EQUAL(get<li>(parse("[1, 2, 3]")), li({1, 2, 3}));
  CAF_CHECK_EQUAL(get<ls>(parse("[\"abc\", \"def\", \"ghi\"]")),
                  ls({"abc", "def", "ghi"}));
  CAF_CHECK_EQUAL(get<lli>(parse("[[1, 2], [3]]")), lli({li{1, 2}, li{3}}));
  CAF_CHECK_EQUAL(get<timespan>(parse("10ms")), milliseconds(10));
  CAF_CHECK_EQUAL(get<di>(parse("{a=1,b=2}")), di({{"a", 1}, {"b", 2}}));
}

CAF_TEST(unsuccessful parsing) {
  auto parse = [](const string& str) {
    auto x = config_value::parse(str);
    if (x)
      CAF_FAIL("assumed an error but got a result");
    return std::move(x.error());
  };
  CAF_CHECK_EQUAL(parse("10msb"), pec::trailing_character);
  CAF_CHECK_EQUAL(parse("10foo"), pec::trailing_character);
  CAF_CHECK_EQUAL(parse("[1,"), pec::unexpected_eof);
  CAF_CHECK_EQUAL(parse("{a=,"), pec::unexpected_character);
  CAF_CHECK_EQUAL(parse("{a=1,"), pec::unexpected_eof);
  CAF_CHECK_EQUAL(parse("{a=1 b=2}"), pec::unexpected_character);
}

CAF_TEST(put values) {
  using v = config_value;
  using d = config_value::dictionary;
  using dd = caf::dictionary<d>;
  dd content;
  put(content, "a.b", 42);
  CAF_CHECK_EQUAL(content, dd({{"a", d({{"b", v{42}}})}}));
  put(content, "a.b.c", 1);
  CAF_CHECK_EQUAL(content, dd({{"a", d({{"b", v{d({{"c", v{1}}})}}})}}));
  put(content, "a.b.d", 2);
  CAF_CHECK_EQUAL(content,
                  dd({{"a", d({{"b", v{d({{"c", v{1}}, {"d", v{2}}})}}})}}));
}
