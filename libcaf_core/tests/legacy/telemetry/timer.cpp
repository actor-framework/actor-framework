// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE telemetry.timer

#include "caf/telemetry/timer.hpp"

#include "core-test.hpp"

using namespace caf;
using namespace caf::telemetry;

CAF_TEST(timers observe how much time passes in a scope) {
  dbl_histogram h1{1, 2, 4, 8};
  MESSAGE("timers call observe() with the measured time");
  {
    timer t{&h1};
    CHECK_EQ(t.histogram_ptr(), &h1);
    CHECK_GT(t.started().time_since_epoch().count(), 0);
  }
  CHECK_GT(h1.sum(), 0.0);
  MESSAGE("timers constructed with a nullptr have no effect");
  {
    timer t{nullptr};
    CHECK_EQ(t.histogram_ptr(), nullptr);
  }
}
