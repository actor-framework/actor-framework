// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.parser.read_floating_point

#include "caf/detail/parser/read_floating_point.hpp"

#include "caf/test/dsl.hpp"

#include <string>

#include "caf/parser_state.hpp"
#include "caf/string_view.hpp"
#include "caf/variant.hpp"

using namespace caf;

namespace {

struct double_consumer {
  using value_type = double;

  void value(double y) {
    x = y;
  }

  double x;
};

std::optional<double> read(string_view str) {
  double_consumer consumer;
  string_parser_state ps{str.begin(), str.end()};
  detail::parser::read_floating_point(ps, consumer);
  if (ps.code != pec::success)
    return std::nullopt;
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
