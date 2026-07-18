// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/daemons.hpp"

#include "caf/test/test.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/detail/actor_system_access.hpp"
#include "caf/event_based_actor.hpp"

using namespace caf;

namespace {

constexpr auto noop_stop = [](actor) {};

} // namespace

TEST("daemons::launch spawns hidden detached workers") {
  actor_system_config cfg;
  put(cfg.content, "caf.scheduler.max-threads", 1);
  actor_system sys{cfg};
  auto* daemons = detail::actor_system_access{sys}.daemons();
  auto worker = daemons->launch([](event_based_actor*) { return behavior{}; },
                                noop_stop);
  check(static_cast<bool>(worker));
}
