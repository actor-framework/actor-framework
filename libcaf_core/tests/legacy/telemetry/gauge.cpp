// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE telemetry.gauge

#include "caf/telemetry/gauge.hpp"

#include "core-test.hpp"

using namespace caf;

CAF_TEST(double gauges can increment and decrement) {
  telemetry::dbl_gauge g;
  MESSAGE("gauges start at 0");
  CHECK_EQ(g.value(), 0.0);
  MESSAGE("gauges are incrementable");
  g.inc();
  g.inc(2.0);
  CHECK_EQ(g.value(), 3.0);
  MESSAGE("gauges are decrementable");
  g.dec();
  g.dec(5.0);
  CHECK_EQ(g.value(), -3.0);
  MESSAGE("gauges allow setting values");
  g.value(42.0);
  CHECK_EQ(g.value(), 42.0);
  MESSAGE("users can create gauges with custom start values");
  CHECK_EQ(telemetry::dbl_gauge{42.0}.value(), 42.0);
}

CAF_TEST(integer gauges can increment and decrement) {
  telemetry::int_gauge g;
  MESSAGE("gauges start at 0");
  CHECK_EQ(g.value(), 0);
  MESSAGE("gauges are incrementable");
  g.inc();
  g.inc(2);
  CHECK_EQ(g.value(), 3);
  MESSAGE("gauges are decrementable");
  g.dec();
  g.dec(5);
  CHECK_EQ(g.value(), -3);
  MESSAGE("gauges allow setting values");
  g.value(42);
  MESSAGE("integer gauges also support operator++");
  CHECK_EQ(++g, 43);
  MESSAGE("users can create gauges with custom start values");
  CHECK_EQ(telemetry::int_gauge{42}.value(), 42);
}
