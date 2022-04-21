// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.parser.read_floating_point

#include "caf/detail/parser/read_floating_point.hpp"

#include "core-test.hpp"

#include <string>
#include <string_view>
#include <variant>

#include "caf/parser_state.hpp"

using namespace caf;

namespace {

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

} // namespace

CAF_TEST(predecimal only) {
  CHECK_EQ(read("0"), 0.);
  CHECK_EQ(read("+0"), 0.);
  CHECK_EQ(read("-0"), 0.);
  CHECK_EQ(read("1"), 1.);
  CHECK_EQ(read("+1"), 1.);
  CHECK_EQ(read("-1"), -1.);
  CHECK_EQ(read("12"), 12.);
  CHECK_EQ(read("+12"), 12.);
  CHECK_EQ(read("-12"), -12.);
}

CAF_TEST(trailing dot) {
  CHECK_EQ(read("0."), 0.);
  CHECK_EQ(read("1."), 1.);
  CHECK_EQ(read("+1."), 1.);
  CHECK_EQ(read("-1."), -1.);
  CHECK_EQ(read("12."), 12.);
  CHECK_EQ(read("+12."), 12.);
  CHECK_EQ(read("-12."), -12.);
}

CAF_TEST(leading dot) {
  CHECK_EQ(read(".0"), .0);
  CHECK_EQ(read(".1"), .1);
  CHECK_EQ(read("+.1"), .1);
  CHECK_EQ(read("-.1"), -.1);
  CHECK_EQ(read(".12"), .12);
  CHECK_EQ(read("+.12"), .12);
  CHECK_EQ(read("-.12"), -.12);
}

CAF_TEST(regular noation) {
  CHECK_EQ(read("0.0"), .0);
  CHECK_EQ(read("1.2"), 1.2);
  CHECK_EQ(read("1.23"), 1.23);
  CHECK_EQ(read("12.34"), 12.34);
}

CAF_TEST(scientific noation) {
  CHECK_EQ(read("1e2"), 1e2);
  CHECK_EQ(read("+1e2"), 1e2);
  CHECK_EQ(read("+1e+2"), 1e2);
  CHECK_EQ(read("-1e2"), -1e2);
  CHECK_EQ(read("-1e+2"), -1e2);
  CHECK_EQ(read("12e-3"), 12e-3);
  CHECK_EQ(read("+12e-3"), 12e-3);
  CHECK_EQ(read("-12e-3"), -12e-3);
}
