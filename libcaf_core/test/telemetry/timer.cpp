// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE telemetry.timer

#include "caf/telemetry/timer.hpp"

#include "caf/test/dsl.hpp"

using namespace caf;
using namespace caf::telemetry;

CAF_TEST(timers observe how much time passes in a scope) {
  dbl_histogram h1{1, 2, 4, 8};
  CAF_MESSAGE("timers call observe() with the measured time");
  {
    timer t{&h1};
    CAF_CHECK_EQUAL(t.histogram_ptr(), &h1);
    CAF_CHECK_GREATER(t.started().time_since_epoch().count(), 0);
  }
  CAF_CHECK_GREATER(h1.sum(), 0.0);
  CAF_MESSAGE("timers constructed with a nullptr have no effect");
  {
    timer t{nullptr};
    CAF_CHECK_EQUAL(t.histogram_ptr(), nullptr);
  }
}
