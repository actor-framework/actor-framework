// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/abstract_actor.hpp"

#include "caf/test/test.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"

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

TEST("abstract_actor::current returns the scoped_actor pointer") {
  actor_system_config cfg;
  actor_system sys{cfg};
  scoped_actor self{sys};
  check_eq(abstract_actor::current(), self.ptr());
}

TEST("current reflects context changes with nested scoped_actors") {
  actor_system_config cfg;
  actor_system sys{cfg};
  auto* before = abstract_actor::current();
  check_eq(before, nullptr);
  {
    scoped_actor outer{sys};
    check_eq(abstract_actor::current(), outer.ptr());
    {
      scoped_actor inner{sys};
      check_eq(abstract_actor::current(), inner.ptr());
    }
    // After inner is destroyed, current should be restored to outer.
    check_eq(abstract_actor::current(), outer.ptr());
  }
  // After outer is destroyed, current should be restored to nullptr.
  check_eq(abstract_actor::current(), before);
}
