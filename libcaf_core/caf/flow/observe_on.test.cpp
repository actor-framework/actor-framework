// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/flow/observable_builder.hpp"

#include <memory>

using namespace caf;

namespace {

WITH_FIXTURE(test::fixture::deterministic) {

SCENARIO("observe_on moves data between actors") {
  GIVEN("a generation") {
    WHEN("calling observe_on") {
      THEN("the target actor observes all values") {
        auto inputs = std::vector<int>{1, 2, 4, 8, 16, 32, 64, 128};
        auto outputs = std::vector<int>{};
        auto [src, launch_src] = sys.spawn_inactive();
        auto [snk, launch_snk] = sys.spawn_inactive();
        src->make_observable()
          .from_container(inputs)
          .filter([](int) { return true; })
          .observe_on(snk)
          .for_each([&outputs](int x) { outputs.emplace_back(x); });
        launch_src();
        launch_snk();
        dispatch_messages();
        check_eq(inputs, outputs);
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
