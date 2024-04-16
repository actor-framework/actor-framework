// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_from_state.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_actor_pointer.hpp"
#include "caf/typed_event_based_actor.hpp"

using namespace caf;
using namespace std::literals;

namespace {

behavior int_behavior() {
  return {
    [](int) {},
  };
}

using int_actor = typed_actor<result<void>(int)>;

using int_actor_ptr = int_actor::pointer_view;

struct int_actor_state {
  using init_fn = std::function<void(int_actor_ptr)>;

  int_actor_state(int_actor_ptr ptr, init_fn fn)
    : self(ptr), init(std::move(fn)) {
    // nop
  }

  int_actor::behavior_type make_behavior() {
    init(self);
    return {
      [](int) {},
    };
  }

  int_actor_ptr self;
  init_fn init;
};

struct fixture : test::fixture::deterministic {
  int_actor spawn_int_actor(int_actor_state::init_fn init) {
    return sys.spawn(actor_from_state<int_actor_state>, std::move(init));
  }
};

WITH_FIXTURE(fixture) {

SCENARIO("run_delayed triggers an action after a relative timeout") {
  GIVEN("a scheduled actor") {
    WHEN("the actor schedules an action with run_delayed") {
      THEN("the action triggers after the relative timeout") {
        auto called = std::make_shared<bool>(false);
        auto aut = sys.spawn([called](event_based_actor* self) {
          self->run_delayed(1s, [called] { *called = true; });
          return int_behavior();
        });
        dispatch_messages();
        check(!*called);
        advance_time(1s);
        dispatch_messages();
        check(*called);
      }
      AND_THEN("disposing the pending timeout cancels the action") {
        auto called = std::make_shared<bool>(false);
        auto pending = disposable{};
        auto aut = sys.spawn([called, &pending](event_based_actor* self) {
          pending = self->run_delayed(1s, [called] { *called = true; });
          return int_behavior();
        });
        dispatch_messages();
        check(!*called);
        pending.dispose();
        advance_time(1s);
        dispatch_messages();
        check(!*called);
      }
    }
  }
  GIVEN("a typed actor") {
    WHEN("the actor schedules an action with run_delayed") {
      THEN("the action triggers after the relative timeout") {
        auto called = std::make_shared<bool>(false);
        auto aut = spawn_int_actor([called](int_actor_ptr self) {
          self->run_delayed(1s, [called] { *called = true; });
        });
        dispatch_messages();
        check(!*called);
        advance_time(1s);
        dispatch_messages();
        check(*called);
      }
      AND_THEN("disposing the pending timeout cancels the action") {
        auto called = std::make_shared<bool>(false);
        auto pending = disposable{};
        auto aut = spawn_int_actor([called, &pending](int_actor_ptr self) {
          pending = self->run_delayed(1s, [called] { *called = true; });
        });
        dispatch_messages();
        check(!*called);
        pending.dispose();
        advance_time(1s);
        dispatch_messages();
        check(!*called);
      }
    }
  }
}

SCENARIO("run_delayed_weak triggers an action after a relative timeout") {
  GIVEN("a scheduled actor") {
    WHEN("the actor schedules an action with run_delayed") {
      THEN("the action triggers after the relative timeout for live actors") {
        auto called = std::make_shared<bool>(false);
        auto aut = sys.spawn([called](event_based_actor* self) {
          self->run_delayed_weak(1s, [called] { *called = true; });
          return int_behavior();
        });
        dispatch_messages();
        check(!*called);
        advance_time(1s);
        dispatch_messages();
        check(*called);
      }
      AND_THEN("no action triggers for terminated actors") {
        auto called = std::make_shared<bool>(false);
        sys.spawn([called](event_based_actor* self) {
          self->run_delayed_weak(1s, [called] { *called = true; });
          return int_behavior();
        });
        dispatch_messages(); // Note: actor cleaned up after this line.
        check(!*called);
        advance_time(1s);
        dispatch_messages();
        check(!*called);
      }
      AND_THEN("disposing the pending timeout cancels the action") {
        auto called = std::make_shared<bool>(false);
        auto pending = disposable{};
        auto aut = sys.spawn([called, &pending](event_based_actor* self) {
          pending = self->run_delayed_weak(1s, [called] { *called = true; });
          return int_behavior();
        });
        dispatch_messages();
        check(!*called);
        pending.dispose();
        advance_time(1s);
        dispatch_messages();
        check(!*called);
      }
    }
  }
  GIVEN("a typed actor") {
    WHEN("the actor schedules an action with run_delayed") {
      THEN("the action triggers after the relative timeout for live actors") {
        auto called = std::make_shared<bool>(false);
        auto aut = spawn_int_actor([called](int_actor_ptr self) {
          self->run_delayed_weak(1s, [called] { *called = true; });
        });
        dispatch_messages();
        check(!*called);
        advance_time(1s);
        dispatch_messages();
        check(*called);
      }
      AND_THEN("no action triggers for terminated actors") {
        auto called = std::make_shared<bool>(false);
        spawn_int_actor([called](int_actor_ptr self) {
          self->run_delayed_weak(1s, [called] { *called = true; });
        });
        dispatch_messages(); // Note: actor cleaned up after this line.
        check(!*called);
        advance_time(1s);
        dispatch_messages();
        check(!*called);
      }
      AND_THEN("disposing the pending timeout cancels the action") {
        auto called = std::make_shared<bool>(false);
        auto pending = disposable{};
        auto aut = spawn_int_actor([called, &pending](int_actor_ptr self) {
          pending = self->run_delayed_weak(1s, [called] { *called = true; });
        });
        dispatch_messages();
        check(!*called);
        pending.dispose();
        advance_time(1s);
        dispatch_messages();
        check(!*called);
      }
    }
  }
}

} // WITH_FIXTURE(fixture)

} // namespace
