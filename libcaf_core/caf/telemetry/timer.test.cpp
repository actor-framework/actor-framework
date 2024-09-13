// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/telemetry/timer.hpp"

#include "caf/test/test.hpp"

using namespace caf;
using namespace caf::telemetry;

TEST("timers observe how much time passes in a scope") {
  SECTION("timers call observe() with the measured time") {
    dbl_histogram h1{1, 2, 4, 8};
    {
      timer t{&h1};
      check_eq(t.histogram_ptr(), &h1);
      check_gt(t.started().time_since_epoch().count(), 0);
      // timer adds to h1 at scope exit
    }
    check_gt(h1.sum(), 0.0);
  }
  SECTION("timers constructed with a nullptr have no effect") {
    timer t{nullptr};
    check_eq(t.histogram_ptr(), nullptr);
  }
}
