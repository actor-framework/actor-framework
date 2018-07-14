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
  auto res = config_value::parse(arg);
  if (res && holds_alternative<T>(*res)) {
    CAF_CHECK_EQUAL(co.check(*res), none);
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

} // namespace <anonymous>

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
  CAF_CHECK_EQUAL(unbox(read<float>("-0.1")),  -0.1f);
  CAF_CHECK_EQUAL(read<float>("0"),  none);
  CAF_CHECK_EQUAL(read<float>("\"0.1\""),  none);
}

CAF_TEST(type double) {
  CAF_CHECK_EQUAL(unbox(read<double>("-1.0")),  -1.0);
  CAF_CHECK_EQUAL(unbox(read<double>("-0.1")),  -0.1);
  CAF_CHECK_EQUAL(read<double>("0"),  none);
  CAF_CHECK_EQUAL(read<double>("\"0.1\""),  none);
}

CAF_TEST(type string) {
  CAF_CHECK_EQUAL(unbox(read<string>("\"foo\"")), "foo");
  CAF_CHECK_EQUAL(unbox(read<string>("foo")), "foo");
}

CAF_TEST(type atom) {
  CAF_CHECK_EQUAL(unbox(read<atom_value>("'foo'")), atom("foo"));
  CAF_CHECK_EQUAL(read<atom_value>("bar"), none);
}

CAF_TEST(type timespan) {
  timespan dur{500};
  CAF_CHECK_EQUAL(unbox(read<timespan>("500ns")), dur);
}
