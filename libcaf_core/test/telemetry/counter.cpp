// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE telemetry.counter

#include "caf/telemetry/counter.hpp"

#include "caf/test/dsl.hpp"

using namespace caf;

CAF_TEST(double counters can only increment) {
  telemetry::dbl_counter c;
  CAF_MESSAGE("counters start at 0");
  CAF_CHECK_EQUAL(c.value(), 0.0);
  CAF_MESSAGE("counters are incrementable");
  c.inc();
  c.inc(2.0);
  CAF_CHECK_EQUAL(c.value(), 3.0);
  CAF_MESSAGE("users can create counters with custom start values");
  CAF_CHECK_EQUAL(telemetry::dbl_counter{42.0}.value(), 42.0);
}

CAF_TEST(integer counters can only increment) {
  telemetry::int_counter c;
  CAF_MESSAGE("counters start at 0");
  CAF_CHECK_EQUAL(c.value(), 0);
  CAF_MESSAGE("counters are incrementable");
  c.inc();
  c.inc(2);
  CAF_CHECK_EQUAL(c.value(), 3);
  CAF_MESSAGE("integer counters also support operator++");
  CAF_CHECK_EQUAL(++c, 4);
  CAF_MESSAGE("users can create counters with custom start values");
  CAF_CHECK_EQUAL(telemetry::int_counter{42}.value(), 42);
}
