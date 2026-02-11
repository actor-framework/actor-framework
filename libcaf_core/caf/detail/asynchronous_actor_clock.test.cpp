// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/asynchronous_actor_clock.hpp"

#include "caf/test/test.hpp"

#include "caf/action.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/telemetry/metric_registry.hpp"

#include <chrono>

using namespace caf;
using namespace std::chrono_literals;

TEST("enqueueing a job to the clock executes it after the timeout") {
  actor_system_config cfg;
  actor_system sys{cfg};
  auto& clock = sys.clock();
  auto* actions = sys.metrics().counter_singleton("test", "actions", "test");
  clock.schedule(clock.now() + 1ms,
                 make_single_shot_action([actions] { actions->inc(); }));
  check(sys.metrics().wait_for("test", "actions", 1s, 10ms,
                               [](int64_t x) { return x == 1; }));
}

TEST("setting a cleanup interval removes disposed jobs from the clock") {
  actor_system_config cfg;
  cfg.set("caf.clock.cleanup-interval", "5ms");
  actor_system sys{cfg};
  auto& clock = sys.clock();
  auto* actions = sys.metrics().counter_singleton("test", "actions", "test");
  auto hdl = clock.schedule(clock.now() + 1h, make_single_shot_action(
                                                [actions] { actions->inc(); }));
  require(sys.metrics().wait_for("caf.system", "actor-clock-queue-size", 1s,
                                 10ms, [](int64_t x) { return x == 1; }));
  hdl.dispose();
  require(sys.metrics().wait_for("caf.system", "actor-clock-queue-size", 1s,
                                 10ms, [](int64_t x) { return x == 0; }));
  check_eq(actions->value(), 0);
}
