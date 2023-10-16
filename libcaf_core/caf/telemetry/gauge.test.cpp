// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/telemetry/gauge.hpp"

#include "caf/test/test.hpp"

using namespace caf;

namespace {

TEST("gauges can increment and decrement") {
  SECTION("double gauges") {
    telemetry::dbl_gauge g;
    SECTION("gauges start at 0") {
      check_eq(g.value(), 0.0);
    }
    SECTION("gauges are incrementable") {
      g.inc();
      g.inc(2.0);
      check_eq(g.value(), 3.0);
    }
    SECTION("gauges are decrementable") {
      g.dec();
      g.dec(5.0);
      check_eq(g.value(), -6.0);
    }
    SECTION("gauges allow setting values") {
      g.value(42.0);
      check_eq(g.value(), 42.0);
    }
    SECTION("users can create gauges with custom start values") {
      check_eq(telemetry::dbl_gauge{42.0}.value(), 42.0);
    }
  }
  SECTION("integer gauges") {
    telemetry::int_gauge g;
    SECTION("gauges start at 0") {
      check_eq(g.value(), 0);
    }
    SECTION("gauges are incrementable") {
      g.inc();
      g.inc(2);
      check_eq(g.value(), 3);
      check_eq(++g, 4);
      check_eq(g++, 4);
      check_eq(g.value(), 5);
    }
    SECTION("gauges are decrementable") {
      g.dec();
      g.dec(5);
      check_eq(g.value(), -6);
      check_eq(--g, -7);
      check_eq(g--, -7);
      check_eq(g.value(), -8);
    }
    SECTION("gauges allow setting values") {
      g.value(42);
    }
    SECTION("users can create gauges with custom start values") {
      check_eq(telemetry::int_gauge{42}.value(), 42);
    }
  }
}

} // namespace
