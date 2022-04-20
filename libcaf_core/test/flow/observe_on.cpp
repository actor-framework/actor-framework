// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.observe_on

#include "caf/flow/observable.hpp"

#include "core-test.hpp"

#include <memory>

#include "caf/flow/coordinator.hpp"
#include "caf/flow/merge.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/observer.hpp"
#include "caf/scheduled_actor/flow.hpp"

using namespace caf;

BEGIN_FIXTURE_SCOPE(test_coordinator_fixture<>)

SCENARIO("observe_on moves data between actors") {
  GIVEN("a generation") {
    WHEN("calling observe_on") {
      THEN("the target actor observes all values") {
        using actor_t = event_based_actor;
        auto inputs = std::vector<int>{1, 2, 4, 8, 16, 32, 64, 128};
        auto outputs = std::vector<int>{};
        auto [src, launch_src] = sys.spawn_inactive<actor_t>();
        auto [snk, launch_snk] = sys.spawn_inactive<actor_t>();
        src->make_observable()
          .from_container(inputs)
          .filter([](int) { return true; })
          .observe_on(snk)
          .for_each([&outputs](int x) { outputs.emplace_back(x); });
        launch_src();
        launch_snk();
        run();
        CHECK_EQ(inputs, outputs);
      }
    }
  }
}

END_FIXTURE_SCOPE()
