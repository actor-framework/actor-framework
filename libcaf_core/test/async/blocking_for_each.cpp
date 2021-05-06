// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE async.blocking_for_each

#include "caf/async/publisher.hpp"

#include "core-test.hpp"

#include "caf/flow/coordinator.hpp"
#include "caf/flow/observable.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/observer.hpp"
#include "caf/scheduled_actor/flow.hpp"

using namespace caf;

namespace {

struct fixture {
  actor_system_config cfg;
  actor_system sys;

  fixture() : sys(cfg.set("caf.scheduler.max-threads", 2)) {
    // nop
  }
};

using ctx_impl = event_based_actor;

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("blocking_for_each iterates all values in a stream") {
  GIVEN("an asynchronous source") {
    WHEN("subscribing to its output via blocking_for_each") {
      THEN("the observer blocks until it has received all values") {
        auto inputs = std::vector<int>(2539, 42);
        auto outputs = std::vector<int>{};
        async::publisher_from<ctx_impl>(sys, [](auto* self) {
          return self->make_observable().repeat(42).take(2539);
        }).blocking_for_each([&outputs](int x) { outputs.emplace_back(x); });
        CHECK_EQ(inputs, outputs);
      }
    }
  }
}

END_FIXTURE_SCOPE()
