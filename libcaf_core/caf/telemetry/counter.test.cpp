// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/telemetry/counter.hpp"

#include "caf/test/approx.hpp"
#include "caf/test/test.hpp"

using namespace caf;

TEST("counters can only increment") {
  SECTION("double counters") {
    telemetry::dbl_counter c;
    SECTION("counters start at 0") {
      check_eq(c.value(), test::approx{0.0});
    }
    SECTION("counters are incrementable") {
      c.inc();
      c.inc(2.0);
      check_eq(c.value(), test::approx{3.0});
    }
    SECTION("users can create counters with custom start values") {
      check_eq(telemetry::dbl_counter{42.0}.value(), test::approx{42.0});
    }
  }
  SECTION("integer counters") {
    telemetry::int_counter c;
    SECTION("counters start at 0") {
      check_eq(c.value(), 0);
    }
    SECTION("counters are incrementable") {
      c.inc();
      c.inc(2);
      check_eq(c.value(), 3);
      check_eq(++c, 4);
      check_eq(c++, 4);
      check_eq(c.value(), 5);
    }
    SECTION("users can create counters with custom start values") {
      check_eq(telemetry::int_counter{42}.value(), 42);
    }
  }
}
