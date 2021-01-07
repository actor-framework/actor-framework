// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE telemetry.gauge

#include "caf/telemetry/gauge.hpp"

#include "caf/test/dsl.hpp"

using namespace caf;

CAF_TEST(double gauges can increment and decrement) {
  telemetry::dbl_gauge g;
  CAF_MESSAGE("gauges start at 0");
  CAF_CHECK_EQUAL(g.value(), 0.0);
  CAF_MESSAGE("gauges are incrementable");
  g.inc();
  g.inc(2.0);
  CAF_CHECK_EQUAL(g.value(), 3.0);
  CAF_MESSAGE("gauges are decrementable");
  g.dec();
  g.dec(5.0);
  CAF_CHECK_EQUAL(g.value(), -3.0);
  CAF_MESSAGE("gauges allow setting values");
  g.value(42.0);
  CAF_CHECK_EQUAL(g.value(), 42.0);
  CAF_MESSAGE("users can create gauges with custom start values");
  CAF_CHECK_EQUAL(telemetry::dbl_gauge{42.0}.value(), 42.0);
}

CAF_TEST(integer gauges can increment and decrement) {
  telemetry::int_gauge g;
  CAF_MESSAGE("gauges start at 0");
  CAF_CHECK_EQUAL(g.value(), 0);
  CAF_MESSAGE("gauges are incrementable");
  g.inc();
  g.inc(2);
  CAF_CHECK_EQUAL(g.value(), 3);
  CAF_MESSAGE("gauges are decrementable");
  g.dec();
  g.dec(5);
  CAF_CHECK_EQUAL(g.value(), -3);
  CAF_MESSAGE("gauges allow setting values");
  g.value(42);
  CAF_MESSAGE("integer gauges also support operator++");
  CAF_CHECK_EQUAL(++g, 43);
  CAF_MESSAGE("users can create gauges with custom start values");
  CAF_CHECK_EQUAL(telemetry::int_gauge{42}.value(), 42);
}
