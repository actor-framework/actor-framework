// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/parser/read_floating_point.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/test.hpp"

#include "caf/parser_state.hpp"

#include <string>
#include <string_view>
#include <variant>

using namespace caf;

struct double_consumer {
  using value_type = double;

  void value(double y) {
    x = y;
  }

  double x;
};

std::optional<double> read(std::string_view str) {
  double_consumer consumer;
  string_parser_state ps{str.begin(), str.end()};
  detail::parser::read_floating_point(ps, consumer);
  if (ps.code != pec::success)
    return std::nullopt;
  return consumer.x;
}

TEST("predecimal only") {
  check_eq(read("0"), 0.);
  check_eq(read("+0"), 0.);
  check_eq(read("-0"), 0.);
  check_eq(read("1"), 1.);
  check_eq(read("+1"), 1.);
  check_eq(read("-1"), -1.);
  check_eq(read("12"), 12.);
  check_eq(read("+12"), 12.);
  check_eq(read("-12"), -12.);
}

TEST("trailing dot") {
  check_eq(read("0."), 0.);
  check_eq(read("1."), 1.);
  check_eq(read("+1."), 1.);
  check_eq(read("-1."), -1.);
  check_eq(read("12."), 12.);
  check_eq(read("+12."), 12.);
  check_eq(read("-12."), -12.);
}

TEST("leading dot") {
  check_eq(read(".0"), .0);
  check_eq(read(".1"), .1);
  check_eq(read("+.1"), .1);
  check_eq(read("-.1"), -.1);
  check_eq(read(".12"), .12);
  check_eq(read("+.12"), .12);
  check_eq(read("-.12"), -.12);
}

TEST("regular noation") {
  check_eq(read("0.0"), .0);
  check_eq(read("1.2"), 1.2);
  check_eq(read("1.23"), 1.23);
  check_eq(read("12.34"), 12.34);
}

TEST("scientific noation") {
  check_eq(read("1e2"), 1e2);
  check_eq(read("+1e2"), 1e2);
  check_eq(read("+1e+2"), 1e2);
  check_eq(read("-1e2"), -1e2);
  check_eq(read("-1e+2"), -1e2);
  check_eq(read("12e-3"), 12e-3);
  check_eq(read("+12e-3"), 12e-3);
  check_eq(read("-12e-3"), -12e-3);
}

CAF_TEST_MAIN()
