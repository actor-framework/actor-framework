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

#define CAF_SUITE read_floating_point

#include "caf/detail/parser/read_floating_point.hpp"

#include "caf/test/dsl.hpp"

#include <string>

#include "caf/detail/parser/state.hpp"
#include "caf/pec.hpp"

using namespace caf;

namespace {

struct double_consumer {
  using value_type = double;

  void value(double y) {
    x = y;
  }

  double x;
};

optional<double> read(string_view str) {
  double_consumer consumer;
  detail::parser::state<string_view::iterator> ps{str.begin(), str.end()};
  detail::parser::read_floating_point(ps, consumer);
  if (ps.code != pec::success)
    return none;
  return consumer.x;
}

} // namespace

CAF_TEST(predecimal only) {
  CAF_CHECK_EQUAL(read("0"), 0.);
  CAF_CHECK_EQUAL(read("+0"), 0.);
  CAF_CHECK_EQUAL(read("-0"), 0.);
  CAF_CHECK_EQUAL(read("1"), 1.);
  CAF_CHECK_EQUAL(read("+1"), 1.);
  CAF_CHECK_EQUAL(read("-1"), -1.);
  CAF_CHECK_EQUAL(read("12"), 12.);
  CAF_CHECK_EQUAL(read("+12"), 12.);
  CAF_CHECK_EQUAL(read("-12"), -12.);
}

CAF_TEST(trailing dot) {
  CAF_CHECK_EQUAL(read("0."), 0.);
  CAF_CHECK_EQUAL(read("1."), 1.);
  CAF_CHECK_EQUAL(read("+1."), 1.);
  CAF_CHECK_EQUAL(read("-1."), -1.);
  CAF_CHECK_EQUAL(read("12."), 12.);
  CAF_CHECK_EQUAL(read("+12."), 12.);
  CAF_CHECK_EQUAL(read("-12."), -12.);
}

CAF_TEST(leading dot) {
  CAF_CHECK_EQUAL(read(".0"), .0);
  CAF_CHECK_EQUAL(read(".1"), .1);
  CAF_CHECK_EQUAL(read("+.1"), .1);
  CAF_CHECK_EQUAL(read("-.1"), -.1);
  CAF_CHECK_EQUAL(read(".12"), .12);
  CAF_CHECK_EQUAL(read("+.12"), .12);
  CAF_CHECK_EQUAL(read("-.12"), -.12);
}

CAF_TEST(regular noation) {
  CAF_CHECK_EQUAL(read("0.0"), .0);
  CAF_CHECK_EQUAL(read("1.2"), 1.2);
  CAF_CHECK_EQUAL(read("1.23"), 1.23);
  CAF_CHECK_EQUAL(read("12.34"), 12.34);
}

CAF_TEST(scientific noation) {
  CAF_CHECK_EQUAL(read("1e2"), 1e2);
  CAF_CHECK_EQUAL(read("+1e2"), 1e2);
  CAF_CHECK_EQUAL(read("+1e+2"), 1e2);
  CAF_CHECK_EQUAL(read("-1e2"), -1e2);
  CAF_CHECK_EQUAL(read("-1e+2"), -1e2);
  CAF_CHECK_EQUAL(read("12e-3"), 12e-3);
  CAF_CHECK_EQUAL(read("+12e-3"), 12e-3);
  CAF_CHECK_EQUAL(read("-12e-3"), -12e-3);
}
