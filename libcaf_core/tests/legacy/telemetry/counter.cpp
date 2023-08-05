// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE telemetry.counter

#include "caf/telemetry/counter.hpp"

#include "core-test.hpp"

using namespace caf;

CAF_TEST(double counters can only increment) {
  telemetry::dbl_counter c;
  MESSAGE("counters start at 0");
  CHECK_EQ(c.value(), 0.0);
  MESSAGE("counters are incrementable");
  c.inc();
  c.inc(2.0);
  CHECK_EQ(c.value(), 3.0);
  MESSAGE("users can create counters with custom start values");
  CHECK_EQ(telemetry::dbl_counter{42.0}.value(), 42.0);
}

CAF_TEST(integer counters can only increment) {
  telemetry::int_counter c;
  MESSAGE("counters start at 0");
  CHECK_EQ(c.value(), 0);
  MESSAGE("counters are incrementable");
  c.inc();
  c.inc(2);
  CHECK_EQ(c.value(), 3);
  MESSAGE("integer counters also support operator++");
  CHECK_EQ(++c, 4);
  MESSAGE("users can create counters with custom start values");
  CHECK_EQ(telemetry::int_counter{42}.value(), 42);
}
