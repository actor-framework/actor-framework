// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/abstract_actor.hpp"

#include "caf/test/test.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/event_based_actor.hpp"

using namespace caf;

TEST("abstract_actor::current returns nullptr outside of any actor context") {
  check_eq(abstract_actor::current(), nullptr);
}

TEST("abstract_actor::current returns the actor pointer from within an actor") {
  actor_system_config cfg;
  actor_system sys{cfg};
  auto ok = std::make_shared<bool>(false);
  sys.spawn([ok](event_based_actor* self) {
    *ok = (self == abstract_actor::current());
  });
  sys.await_all_actors_done();
  check(*ok);
}
