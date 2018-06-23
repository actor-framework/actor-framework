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

#define CAF_SUITE read_number

#include <string>

#include "caf/test/unit_test.hpp"

#include "caf/variant.hpp"

#include "caf/detail/parser/read_number.hpp"

using namespace caf;

namespace {

struct numbers_parser_consumer {
  variant<int64_t, double> x;
  inline void value(double y) {
    x = y;
  }
  inline void value(int64_t y) {
    x = y;
  }
};

struct res_t {
  variant<pec, double, int64_t> val;
  template <class T>
  res_t(T&& x) : val(std::forward<T>(x)) {
    // nop
  }
};

bool operator==(const res_t& x, const res_t& y) {
  if (x.val.index() != y.val.index())
    return false;
  // Implements a safe equal comparison for double.
  caf::test::equal_to f;
  using caf::get;
  using caf::holds_alternative;
  if (holds_alternative<pec>(x.val))
    return f(get<pec>(x.val), get<pec>(y.val));
  if (holds_alternative<double>(x.val))
    return f(get<double>(x.val), get<double>(y.val));
  return f(get<int64_t>(x.val), get<int64_t>(y.val));
}

struct numbers_parser {
  res_t operator()(std::string str) {
    detail::parser::state<std::string::iterator> res;
    numbers_parser_consumer f;
    res.i = str.begin();
    res.e = str.end();
    detail::parser::read_number(res, f);
    if (res.code == pec::success)
      return f.x;
    return res.code;
  }
};

template <class T>
typename std::enable_if<std::is_integral<T>::value, res_t>::type res(T x) {
  return {static_cast<int64_t>(x)};
}

template <class T>
typename std::enable_if<std::is_floating_point<T>::value, res_t>::type
res(T x) {
  return {static_cast<double>(x)};
}

struct fixture {
  numbers_parser p;
};

} // namespace <anonymous>

#define CHECK_NUMBER(x) CAF_CHECK_EQUAL(p(#x), res(x))

CAF_TEST_FIXTURE_SCOPE(read_number_tests, fixture)

CAF_TEST(binary numbers) {
  /* TODO: use this implementation when switching to C++14
  CHECK_NUMBER(0b0);
  CHECK_NUMBER(0b10);
  CHECK_NUMBER(0b101);
  CHECK_NUMBER(0B1001);
  CHECK_NUMBER(-0b0);
  CHECK_NUMBER(-0b101);
  CHECK_NUMBER(-0B1001);
  */
  CAF_CHECK_EQUAL(p("0b0"), res(0));
  CAF_CHECK_EQUAL(p("0b10"), res(2));
  CAF_CHECK_EQUAL(p("0b101"), res(5));
  CAF_CHECK_EQUAL(p("0B1001"), res(9));
  CAF_CHECK_EQUAL(p("-0b0"), res(0));
  CAF_CHECK_EQUAL(p("-0b101"), res(-5));
  CAF_CHECK_EQUAL(p("-0B1001"), res(-9));
}

CAF_TEST(octal numbers) {
  // valid numbers
  CHECK_NUMBER(00);
  CHECK_NUMBER(010);
  CHECK_NUMBER(0123);
  CHECK_NUMBER(0777);
  CHECK_NUMBER(-00);
  CHECK_NUMBER(-0123);
  // invalid numbers
  CAF_CHECK_EQUAL(p("018"), pec::trailing_character);
}

CAF_TEST(decimal numbers) {
  CHECK_NUMBER(0);
  CHECK_NUMBER(10);
  CHECK_NUMBER(123);
  CHECK_NUMBER(-0);
  CHECK_NUMBER(-123);
}

CAF_TEST(hexadecimal numbers) {
  // valid numbers
  CHECK_NUMBER(0x0);
  CHECK_NUMBER(0x10);
  CHECK_NUMBER(0X123);
  CHECK_NUMBER(0xAF01);
  CHECK_NUMBER(-0x0);
  CHECK_NUMBER(-0x123);
  CHECK_NUMBER(-0xaf01);
  // invalid numbers
  CAF_CHECK_EQUAL(p("0xFG"), pec::trailing_character);
  CAF_CHECK_EQUAL(p("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"),
                  pec::integer_overflow);
  CAF_CHECK_EQUAL(p("-0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"),
                  pec::integer_underflow);
}

CAF_TEST(floating point numbers) {
  CHECK_NUMBER(0.0);
  CHECK_NUMBER(.0);
  CHECK_NUMBER(0.);
  CHECK_NUMBER(0.123);
  CHECK_NUMBER(.123);
  CHECK_NUMBER(123.456);
  CHECK_NUMBER(-0.0);
  CHECK_NUMBER(-.0);
  CHECK_NUMBER(-0.);
  CHECK_NUMBER(-0.123);
  CHECK_NUMBER(-.123);
  CHECK_NUMBER(-123.456);
}

CAF_TEST(integer mantissa with positive exponent) {
  CHECK_NUMBER(321E1);
  CHECK_NUMBER(321e1);
  CHECK_NUMBER(321e+1);
  CHECK_NUMBER(123e2);
  CHECK_NUMBER(-4e2);
  CHECK_NUMBER(1e1);
  CHECK_NUMBER(1e2);
  CHECK_NUMBER(1e3);
  CHECK_NUMBER(1e4);
  CHECK_NUMBER(1e5);
  CHECK_NUMBER(1e6);
}

CAF_TEST(integer mantissa with negative exponent) {
  // valid numbers
  CHECK_NUMBER(321E-1);
  CHECK_NUMBER(321e-1);
  CHECK_NUMBER(123e-2);
  CHECK_NUMBER(-4e-2);
  CHECK_NUMBER(1e-1);
  CHECK_NUMBER(1e-2);
  CHECK_NUMBER(1e-3);
  CHECK_NUMBER(1e-4);
  CHECK_NUMBER(1e-5);
  CHECK_NUMBER(1e-6);
  // invalid numbers
  CAF_CHECK_EQUAL(p("-9.9999e-e511"), pec::unexpected_character);
  CAF_CHECK_EQUAL(p("-9.9999e-511"), pec::exponent_underflow);
}

CAF_TEST(fractional mantissa with positive exponent) {
  CHECK_NUMBER(3.21E1);
  CHECK_NUMBER(3.21e+1);
  CHECK_NUMBER(3.21e+1);
  CHECK_NUMBER(12.3e2);
  CHECK_NUMBER(-0.001e3);
  CHECK_NUMBER(0.0001e5);
  CHECK_NUMBER(-42.001e3);
  CHECK_NUMBER(42.0001e5);
}

CAF_TEST(fractional mantissa with negative exponent) {
  CHECK_NUMBER(3.21E-1);
  CHECK_NUMBER(3.21e-1);
  CHECK_NUMBER(12.3e-2);
  CHECK_NUMBER(-0.001e-3);
  CHECK_NUMBER(-0.0001e-5);
  CHECK_NUMBER(-42.001e-3);
  CHECK_NUMBER(-42001e-6);
  CHECK_NUMBER(-42.0001e-5);
}

CAF_TEST_FIXTURE_SCOPE_END()
