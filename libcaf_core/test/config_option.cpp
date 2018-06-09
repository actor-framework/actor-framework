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

#define CAF_SUITE config_option
#include "caf/test/unit_test.hpp"

#include "caf/config_option.hpp"
#include "caf/actor_system_config.hpp"

// turn off several flags for overflows / sign conversion
#ifdef CAF_CLANG
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wconstant-conversion"
#elif defined(CAF_GCC)
#pragma GCC diagnostic ignored "-Woverflow"
#endif

using namespace caf;

using std::string;

namespace {

constexpr const char* category = "category";
constexpr const char* name = "name";
constexpr const char* explanation = "explanation";
constexpr size_t line = 0;

template<class T>
constexpr int64_t overflow() {
  return static_cast<int64_t>(std::numeric_limits<T>::max()) + 1;
}

template<class T>
constexpr int64_t underflow() {
  return static_cast<int64_t>(std::numeric_limits<T>::min()) - 1;
}

template <class T>
optional<T> read(config_value test_value) {
  T output_value{};
  auto co = make_config_option(output_value, category, name, explanation);
  auto f = co->to_sink();
  if (f(line, test_value, none))
    return output_value;
  return none;
}

// Unsigned integers.
template <class T>
void check_integer_options(std::true_type) {
  // Run tests for positive integers.
  T xzero = 0;
  T xmax = std::numeric_limits<T>::max();
  CAF_CHECK_EQUAL(read<T>(config_value{xzero}), xzero);
  CAF_CHECK_EQUAL(read<T>(config_value{xmax}), xmax);
  CAF_CHECK_EQUAL(read<T>(config_value{overflow<T>()}), none);
}

// Signed integers.
template <class T>
void check_integer_options(std::false_type) {
  // Run tests for positive integers.
  std::true_type tk;
  check_integer_options<T>(tk);
  // Run tests for negative integers.
  auto xmin = std::numeric_limits<T>::min();
  CAF_CHECK_EQUAL(read<T>(config_value{xmin}), xmin);
  CAF_CHECK_EQUAL(read<T>(config_value{underflow<T>()}), none);
}

// only works with an integral types and double
template <class T>
void check_integer_options() {
  std::integral_constant<bool, std::is_unsigned<T>::value> tk;
  check_integer_options<T>(tk);
}

template <class T>
T unbox(optional<T> x) {
  if (!x)
    CAF_FAIL("no value to unbox");
  return std::move(*x);
}

} // namespace <anonymous>

CAF_TEST(type_bool) {
  CAF_CHECK_EQUAL(read<bool>(config_value{true}), true);
  CAF_CHECK_EQUAL(read<bool>(config_value{false}), false);
  CAF_CHECK_EQUAL(read<bool>(config_value{0}), none);
  CAF_CHECK_EQUAL(read<bool>(config_value{1}), none);
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
  CAF_CHECK_EQUAL(unbox(read<uint64_t>(config_value{0})), 0u);
  CAF_CHECK_EQUAL(read<uint64_t>(config_value{-1}), none);
}

CAF_TEST(type int64_t) {
  CAF_CHECK_EQUAL(unbox(read<int64_t>(config_value{-1})), -1);
  CAF_CHECK_EQUAL(unbox(read<int64_t>(config_value{0})),  0);
  CAF_CHECK_EQUAL(unbox(read<int64_t>(config_value{1})),  1);
}

CAF_TEST(type float) {
  CAF_CHECK_EQUAL(unbox(read<float>(config_value{-1.0})),  -1.0f);
  CAF_CHECK_EQUAL(unbox(read<float>(config_value{-0.1})),  -0.1f);
  CAF_CHECK_EQUAL(read<float>(config_value{0}),  none);
  CAF_CHECK_EQUAL(read<float>(config_value{"0.1"}),  none);
}

CAF_TEST(type double) {
  CAF_CHECK_EQUAL(unbox(read<double>(config_value{-1.0})),  -1.0);
  CAF_CHECK_EQUAL(unbox(read<double>(config_value{-0.1})),  -0.1);
  CAF_CHECK_EQUAL(read<double>(config_value{0}),  none);
  CAF_CHECK_EQUAL(read<double>(config_value{"0.1"}),  none);
}

CAF_TEST(type string) {
  CAF_CHECK_EQUAL(unbox(read<string>(config_value{"foo"})), "foo");
}

CAF_TEST(type atom) {
  auto foo = atom("foo");
  CAF_CHECK_EQUAL(unbox(read<atom_value>(config_value{foo})), foo);
  CAF_CHECK_EQUAL(read<atom_value>(config_value{"bar"}), none);
}

CAF_TEST(type timespan) {
  timespan dur{500};
  CAF_CHECK_EQUAL(unbox(read<timespan>(config_value{dur})), dur);
}

template <class T>
std::string name_of() {
  T x{};
  config_option::type_name_visitor v;
  return v(x);
}

CAF_TEST(type_names) {
  CAF_CHECK_EQUAL((name_of<std::map<int, int>>()), "a dictionary");
  CAF_CHECK_EQUAL(name_of<atom_value>(), "an atom_value");
  CAF_CHECK_EQUAL(name_of<bool>(), "a boolean");
  CAF_CHECK_EQUAL(name_of<double>(), "a double");
  CAF_CHECK_EQUAL(name_of<float>(), "a float");
  CAF_CHECK_EQUAL(name_of<int16_t>(), "a 16-bit integer");
  CAF_CHECK_EQUAL(name_of<int32_t>(), "a 32-bit integer");
  CAF_CHECK_EQUAL(name_of<int64_t>(), "a 64-bit integer");
  CAF_CHECK_EQUAL(name_of<int8_t>(), "an 8-bit integer");
  CAF_CHECK_EQUAL(name_of<std::vector<int>>(), "a list");
  CAF_CHECK_EQUAL(name_of<string>(), "a string");
  CAF_CHECK_EQUAL(name_of<uint16_t>(), "a 16-bit unsigned integer");
  CAF_CHECK_EQUAL(name_of<uint32_t>(), "a 32-bit unsigned integer");
  CAF_CHECK_EQUAL(name_of<uint64_t>(), "a 64-bit unsigned integer");
  CAF_CHECK_EQUAL(name_of<uint8_t>(), "an 8-bit unsigned integer");
}
