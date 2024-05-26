// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/beacon.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/flow/scoped_coordinator.hpp"
#include "caf/scheduled_actor/flow.hpp"

using namespace caf;
using namespace std::literals;

namespace {

SCENARIO("beacons can be disposed") {
  auto disposable_beacon = detail::beacon{};
  GIVEN("a beacon") {
    WHEN("dispose() is not called") {
      THEN("the beacon is in a scheduled state") {
        check_eq(disposable_beacon.current_state(), action::state::scheduled);
        check(!disposable_beacon.disposed());
      }
    }
    WHEN("dispose() is called") {
      disposable_beacon.dispose();
      THEN("the beacon is in a disposed state") {
        check_eq(disposable_beacon.current_state(), action::state::disposed);
        check(disposable_beacon.disposed());
      }
    }
  }
}

} // namespace
