/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

namespace {

constexpr const char* category = "category";
constexpr const char* name = "name";
constexpr const char* explanation = "explanation";
constexpr size_t line = 0;

template<class T>
constexpr T zero() { return 0; }

template<class T, class U, class V> 
constexpr T overflow() {
  // +2 is necessary as after an overflow unsigned integral numbers 
  // must differ from zero() in the tests
  return static_cast<V>(std::numeric_limits<U>::max()) + 2;
}

template <class T, class U> 
std::tuple<T, std::string> run_config_option(const T& init_value, 
                                             const U& test_value) {
  std::stringstream ostr;
  T output_value = init_value;
  auto cv = config_value(test_value);
  auto co = make_config_option(output_value, category, name, explanation);
  auto f = co->to_sink(); 
  f(line, cv, static_cast<std::ostream&>(ostr));
  return std::make_tuple(output_value, ostr.str());
}

// only works with an integral types and double 
template <class T, class U> 
void check_num_general_usage() {
  // check positive numbers
  std::string error_str;
  U test_value = 5;
  T result;
  std::tie(result, error_str) = run_config_option(zero<T>(), test_value); 
  CAF_CHECK_EQUAL(result, static_cast<T>(test_value));
  CAF_CHECK(error_str.empty());
  // check negative numbers
  test_value = -5;
  std::tie(result, error_str) = run_config_option(zero<T>(), test_value); 
  if (std::numeric_limits<T>::is_signed) {
    CAF_CHECK_EQUAL(result, static_cast<T>(test_value));
    CAF_CHECK(error_str.empty());
  } else {
    CAF_CHECK_EQUAL(result, zero<T>());
    CAF_CHECK(!error_str.empty());
  }
  // check vector<T>
  test_value = 5;
  std::vector<T> vec_result;
  std::tie(vec_result, error_str) = 
           run_config_option(std::vector<T>{}, test_value);
  CAF_CHECK(!vec_result.empty());
  if (!vec_result.empty()) {
    CAF_CHECK_EQUAL(*vec_result.begin(), static_cast<T>(test_value));
  }
}

// only works with an integral types (no doubles)
template <class T, class U> 
void check_num_boundary_usage() {
  std::string error_str;
  T result;
  U boundary_check = overflow<U,T,U>();
  std::tie(result, error_str) = run_config_option(zero<T>(), boundary_check); 
  T tmp = overflow<T, T, U>();
  CAF_CHECK_NOT_EQUAL(result, tmp);
  CAF_CHECK_EQUAL(result, zero<T>());
  CAF_CHECK(!error_str.empty());
}

// only works with an integral types (no doubles)
template <class T, class U> 
void check_num_general_and_boundary_usage() {
  check_num_general_usage<T, U>();
  check_num_boundary_usage<T, U>();
}

// intended for atoms, strings, and bools 
template <class T> 
void check_non_num_general_usage(const T& init_value, const T& test_value) {
  // general check
  std::string error_str;
  T result;
  std::tie(result, error_str) = run_config_option(init_value, test_value);
  CAF_CHECK_EQUAL(result, test_value);
  // vector<T> check
  std::vector<T> vec_result;
  std::tie(vec_result, error_str) = 
           run_config_option(std::vector<T>{}, test_value);
  CAF_CHECK(!vec_result.empty());
  if (!vec_result.empty()) {
    CAF_CHECK_EQUAL(*vec_result.begin(), test_value);
  }
}

void check_non_num_general_usage(bool init_value, bool test_value) {
  // general check
  std::string error_str;
  bool result;
  std::tie(result, error_str) = run_config_option(init_value, test_value);
  CAF_CHECK_EQUAL(result, test_value);
  // vector<T> check
  // emplace_back() in class cli_arg do not support <bool> until C++14
}

}

CAF_TEST(type_bool) {
  check_non_num_general_usage(false, true);
}

CAF_TEST(type_int8_t) {
  check_num_general_and_boundary_usage<int8_t, int64_t>();
}

CAF_TEST(type_uint8_t) {
  check_num_general_and_boundary_usage<uint8_t, int64_t>();
}

CAF_TEST(type_int16_t) {
  check_num_general_and_boundary_usage<int16_t, int64_t>();
}

CAF_TEST(type_uint16_t) {
  check_num_general_and_boundary_usage<uint16_t, int64_t>();
}

CAF_TEST(type_int32_t) {
  check_num_general_and_boundary_usage<int32_t, int64_t>();
}

CAF_TEST(type_uint32_t) {
  check_num_general_and_boundary_usage<uint32_t, int64_t>();
}

CAF_TEST(type_uint64_t) {
  check_num_general_usage<uint64_t, int64_t>();
}

CAF_TEST(type_int64_t) {
  check_num_general_usage<int64_t, int64_t>();
}

CAF_TEST(type_float) {
  check_num_general_usage<float, double>();
  // check boundaries
  std::string error_str;
  float result;
  float init_value = 0;
  // *2 is required as +2 does not change the variable at this size anymore
  double boundary_check = static_cast<double>(
                                      std::numeric_limits<float>::max()) * 2;
  std::tie(result, error_str) = run_config_option(init_value, boundary_check); 
  float float_inf = std::numeric_limits<float>::infinity();
  // Unit test does not compare inf values correct until now
  bool tmp = float_inf == result;
  CAF_CHECK_NOT_EQUAL(tmp, true);
  CAF_CHECK_EQUAL(result, init_value);
  CAF_CHECK_EQUAL(error_str.empty(), false);
}

CAF_TEST(type_double) {
  check_num_general_usage<double, double>();
}

CAF_TEST(type_string) {
  check_non_num_general_usage<std::string>("", "test string");
}

CAF_TEST(type_atom) {
  // TODO: in class cli_arg std::istringstream do not support atom_value
  // check_non_num_general_usage<atom_value>(atom(""), atom("test atom"));
}

template <class T>
std::string v(const T& x) {
  config_option::type_name_visitor v;
  return v(x);
}

CAF_TEST(type_names) {
  CAF_CHECK_EQUAL(v(true),             "a boolean");
  CAF_CHECK_EQUAL(v(atom("")),         "an atom_value");
  CAF_CHECK_EQUAL(v(std::string{}),    "a string");
  CAF_CHECK_EQUAL(v(zero<float>()),    "a float");
  CAF_CHECK_EQUAL(v(zero<double>()),   "a double");
  CAF_CHECK_EQUAL(v(zero<int8_t>()),   "an 8-bit integer");
  CAF_CHECK_EQUAL(v(zero<uint8_t>()),  "an 8-bit unsigned integer");
  CAF_CHECK_EQUAL(v(zero<int16_t>()),  "a 16-bit integer");
  CAF_CHECK_EQUAL(v(zero<uint16_t>()), "a 16-bit unsigned integer");
  CAF_CHECK_EQUAL(v(zero<int32_t>()),  "a 32-bit integer");
  CAF_CHECK_EQUAL(v(zero<uint32_t>()), "a 32-bit unsigned integer");
  CAF_CHECK_EQUAL(v(zero<int64_t>()),  "a 64-bit integer");
  CAF_CHECK_EQUAL(v(zero<uint64_t>()), "a 64-bit unsigned integer");
}
