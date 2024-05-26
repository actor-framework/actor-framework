// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/typed_actor_view.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_from_state.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scheduled_actor/flow.hpp"
#include "caf/typed_actor_pointer.hpp"
#include "caf/typed_event_based_actor.hpp"

using namespace caf;
using namespace std::literals;

namespace {

using shared_int = std::shared_ptr<int>;

using shared_err = std::shared_ptr<error>;

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

void stream_observer(event_based_actor* self, stream str, shared_int res,
                     shared_err err) {
  // Access `self` through the view to check correct forwarding of `observe_as`.
  auto view = typed_actor_view<result<void>(int)>{self};
  view.observe_as<int>(str, 30, 10)
    .do_on_error([err](const error& what) { *err = what; })
    .for_each([res](int value) { *res += value; });
}

void typed_stream_observer(event_based_actor* self, typed_stream<int> str,
                           shared_int res, shared_err err) {
  // Access `self` through the view to check correct forwarding of `observe`.
  auto view = typed_actor_view<result<void>(int)>{self};
  view.observe(str, 30, 10)
    .do_on_error([err](const error& what) { *err = what; })
    .for_each([res](int value) { *res += value; });
}

struct fixture : test::fixture::deterministic {
  int_actor spawn_int_actor(int_actor_state::init_fn init) {
    return sys.spawn(actor_from_state<int_actor_state>, std::move(init));
  }
};

WITH_FIXTURE(fixture) {

SCENARIO("typed actors may use the flow API") {
  GIVEN("a typed actor") {
    WHEN("the actor calls make_observable") {
      THEN("the function returns a flow that lives on the typed actor") {
        auto res = std::make_shared<int>(0);
        spawn_int_actor([=](int_actor_ptr self) {
          self
            ->make_observable() //
            .iota(1)
            .take(10)
            .for_each([res](int value) { *res += value; });
        });
        dispatch_messages();
        check_eq(*res, 55);
      }
    }
    WHEN("the actor creates a stream via to_stream") {
      THEN("other actors may observe the values") {
        auto res = std::make_shared<int>(0);
        auto err = std::make_shared<error>();
        spawn_int_actor([=](int_actor_ptr self) {
          auto str = self
                       ->make_observable() //
                       .iota(1)
                       .take(10)
                       .to_stream("foo", 10ms, 10);
          self->spawn(stream_observer, str, res, err);
        });
        dispatch_messages();
        check_eq(*res, 55);
        check(!*err);
      }
    }
    WHEN("the actor creates a typed stream via to_typed_stream") {
      THEN("other actors may observe the values") {
        auto res = std::make_shared<int>(0);
        auto err = std::make_shared<error>();
        spawn_int_actor([=](int_actor_ptr self) {
          auto str = self
                       ->make_observable() //
                       .iota(1)
                       .take(10)
                       .to_typed_stream("foo", 10ms, 10);
          self->spawn(typed_stream_observer, str, res, err);
        });
        dispatch_messages();
        check_eq(*res, 55);
        check(!*err);
      }
    }
  }
  WHEN("the actor creates a stream and deregisters it immediately") {
    THEN("other actors receive an error when observing the stream") {
      auto res = std::make_shared<int>(0);
      auto err = std::make_shared<error>();
      spawn_int_actor([=](int_actor_ptr self) {
        auto str = self
                     ->make_observable() //
                     .iota(1)
                     .take(10)
                     .to_stream("foo", 10ms, 10);
        self->spawn(stream_observer, str, res, err);
        self->deregister_stream(str.id());
      });
      dispatch_messages();
      check_eq(*res, 0);
      check_eq(*err, sec::invalid_stream);
    }
  }
}

} // WITH_FIXTURE(fixture)

} // namespace
