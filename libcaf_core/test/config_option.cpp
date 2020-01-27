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

#define CAF_SUITE config_option
#include "caf/test/dsl.hpp"

#include "caf/config_option.hpp"
#include "caf/make_config_option.hpp"
#include "caf/config_value.hpp"
#include "caf/expected.hpp"

using namespace caf;

using std::string;

namespace {

constexpr string_view category = "category";
constexpr string_view name = "name";
constexpr string_view explanation = "explanation";

template<class T>
constexpr int64_t overflow() {
  return static_cast<int64_t>(std::numeric_limits<T>::max()) + 1;
}

template<class T>
constexpr int64_t underflow() {
  return static_cast<int64_t>(std::numeric_limits<T>::min()) - 1;
}

template <class T>
optional<T> read(string_view arg) {
  auto co = make_config_option<T>(category, name, explanation);
  auto res = co.parse(arg);
  if (res && holds_alternative<T>(*res)) {
    if (co.check(*res) != none)
      CAF_ERROR("co.parse() produced the wrong type!");
    return get<T>(*res);
  }
  return none;
}

// Unsigned integers.
template <class T>
void check_integer_options(std::true_type) {
  using std::to_string;
  // Run tests for positive integers.
  T xzero = 0;
  T xmax = std::numeric_limits<T>::max();
  CAF_CHECK_EQUAL(read<T>(to_string(xzero)), xzero);
  CAF_CHECK_EQUAL(read<T>(to_string(xmax)), xmax);
  CAF_CHECK_EQUAL(read<T>(to_string(overflow<T>())), none);
}

// Signed integers.
template <class T>
void check_integer_options(std::false_type) {
  using std::to_string;
  // Run tests for positive integers.
  std::true_type tk;
  check_integer_options<T>(tk);
  // Run tests for negative integers.
  auto xmin = std::numeric_limits<T>::min();
  CAF_CHECK_EQUAL(read<T>(to_string(xmin)), xmin);
  CAF_CHECK_EQUAL(read<T>(to_string(underflow<T>())), none);
}

// only works with an integral types and double
template <class T>
void check_integer_options() {
  std::integral_constant<bool, std::is_unsigned<T>::value> tk;
  check_integer_options<T>(tk);
}

void compare(const config_option& lhs, const config_option& rhs) {
  CAF_CHECK_EQUAL(lhs.category(), rhs.category());
  CAF_CHECK_EQUAL(lhs.long_name(), rhs.long_name());
  CAF_CHECK_EQUAL(lhs.short_names(), rhs.short_names());
  CAF_CHECK_EQUAL(lhs.description(), rhs.description());
  CAF_CHECK_EQUAL(lhs.full_name(), rhs.full_name());
}

} // namespace

CAF_TEST(copy constructor) {
  auto one = make_config_option<int>("cat1", "one", "option 1");
  auto two = one;
  compare(one, two);
}

CAF_TEST(copy assignment) {
  auto one = make_config_option<int>("cat1", "one", "option 1");
  auto two = make_config_option<int>("cat2", "two", "option 2");
  two = one;
  compare(one, two);
}

CAF_TEST(type_bool) {
  CAF_CHECK_EQUAL(read<bool>("true"), true);
  CAF_CHECK_EQUAL(read<bool>("false"), false);
  CAF_CHECK_EQUAL(read<bool>("0"), none);
  CAF_CHECK_EQUAL(read<bool>("1"), none);
}

CAF_TEST(type int8_t) {
  check_integer_options<int8_t>();
}

CAF_TEST(type uint8_t) {
  check_integer_options<uint8_t>();
}

CAF_TEST(type int16_t) {
  check_integer_options<int16_t>();
}

CAF_TEST(type uint16_t) {
  check_integer_options<uint16_t>();
}

CAF_TEST(type int32_t) {
  check_integer_options<int32_t>();
}

CAF_TEST(type uint32_t) {
  check_integer_options<uint32_t>();
}

CAF_TEST(type uint64_t) {
  CAF_CHECK_EQUAL(unbox(read<uint64_t>("0")), 0u);
  CAF_CHECK_EQUAL(read<uint64_t>("-1"), none);
}

CAF_TEST(type int64_t) {
  CAF_CHECK_EQUAL(unbox(read<int64_t>("-1")), -1);
  CAF_CHECK_EQUAL(unbox(read<int64_t>("0")),  0);
  CAF_CHECK_EQUAL(unbox(read<int64_t>("1")),  1);
}

CAF_TEST(type float) {
  CAF_CHECK_EQUAL(unbox(read<float>("-1.0")),  -1.0f);
  CAF_CHECK_EQUAL(unbox(read<float>("-0.1")), -0.1f);
  CAF_CHECK_EQUAL(read<float>("0"), 0.f);
  CAF_CHECK_EQUAL(read<float>("\"0.1\""),  none);
}

CAF_TEST(type double) {
  CAF_CHECK_EQUAL(unbox(read<double>("-1.0")),  -1.0);
  CAF_CHECK_EQUAL(unbox(read<double>("-0.1")),  -0.1);
  CAF_CHECK_EQUAL(read<double>("0"), 0.);
  CAF_CHECK_EQUAL(read<double>("\"0.1\""),  none);
}

CAF_TEST(type string) {
  CAF_CHECK_EQUAL(unbox(read<string>("foo")), "foo");
  CAF_CHECK_EQUAL(unbox(read<string>("\"foo\"")), "foo");
}

CAF_TEST(type timespan) {
  timespan dur{500};
  CAF_CHECK_EQUAL(unbox(read<timespan>("500ns")), dur);
}

CAF_TEST(lists) {
  using int_list = std::vector<int>;
  CAF_CHECK_EQUAL(read<int_list>("[]"), int_list({}));
  CAF_CHECK_EQUAL(read<int_list>("1, 2, 3"), int_list({1, 2, 3}));
  CAF_CHECK_EQUAL(read<int_list>("[1, 2, 3]"), int_list({1, 2, 3}));
}

CAF_TEST(flat CLI parsing) {
  auto x = make_config_option<std::string>("?foo", "bar,b", "test option");
  CAF_CHECK_EQUAL(x.category(), "foo");
  CAF_CHECK_EQUAL(x.long_name(), "bar");
  CAF_CHECK_EQUAL(x.short_names(), "b");
  CAF_CHECK_EQUAL(x.full_name(), "foo.bar");
  CAF_CHECK_EQUAL(x.has_flat_cli_name(), true);
}

CAF_TEST(flat CLI parsing with nested categories) {
  auto x = make_config_option<std::string>("?foo.goo", "bar,b", "test option");
  CAF_CHECK_EQUAL(x.category(), "foo.goo");
  CAF_CHECK_EQUAL(x.long_name(), "bar");
  CAF_CHECK_EQUAL(x.short_names(), "b");
  CAF_CHECK_EQUAL(x.full_name(), "foo.goo.bar");
  CAF_CHECK_EQUAL(x.has_flat_cli_name(), true);
}

CAF_TEST(find by long opt) {
  auto needle = make_config_option<std::string>("?foo", "bar,b", "test option");
  auto check = [&](std::vector<string> args, bool found_opt, bool has_opt) {
    auto res = find_by_long_name(needle, std::begin(args), std::end(args));
    CAF_CHECK_EQUAL(res.first != std::end(args), found_opt);
    if (has_opt)
      CAF_CHECK_EQUAL(res.second, "val2");
    else
      CAF_CHECK(res.second.empty());
  };
  // Well formed, find val2.
  check({"--foo=val1", "--bar=val2", "--baz=val3"}, true, true);
  // Dashes missing, no match.
  check({"--foo=val1", "bar=val2", "--baz=val3"}, false, false);
  // Equal missing.
  check({"--fooval1", "--barval2", "--bazval3"}, false, false);
  // Option value missing.
  check({"--foo=val1", "--bar=", "--baz=val3"}, true, false);
  // With prefix 'caf#'.
  check({"--caf#foo=val1", "--caf#bar=val2", "--caf#baz=val3"}, true, true);
  // Option not included.
  check({"--foo=val1", "--b4r=val2", "--baz=val3"}, false, false);
  // Option not included, with prefix.
  check({"--caf#foo=val1", "--caf#b4r=val2", "--caf#baz=val3"}, false, false);
  // No options to look through.
  check({}, false, false);
}
