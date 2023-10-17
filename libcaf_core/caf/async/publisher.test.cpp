// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/async/publisher.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/scheduled_actor/flow.hpp"

using namespace caf;

namespace {

/*
WITH_FIXTURE(test::fixture::flow) {

SCENARIO("actors can subscribe to their own publishers") {
  GIVEN("an observable") {
    WHEN("converting it to a publisher") {
      THEN("the owner can subscribe to it") {
        auto values = std::make_shared<std::vector<int>>();
        make_observable()
          .iota(1)
          .take(7)
          .to_publisher()
          .observe_on(this_coordinator())
          .do_on_error([this](const error& what) { //
            fail("error: ", to_string(what));
          })
          .for_each([values](int x) { values->push_back(x); });
        run_flows();
        check_eq(*values, std::vector({1, 2, 3, 4, 5, 6, 7}));
      }
    }
  }
}

SCENARIO("default-constructed publishers are invalid") {
  GIVEN("a default-constructed publisher") {
    WHEN("an actor subscribes to it") {
      THEN("the actor observes an invalid_observable error") {
        auto err = std::make_shared<error>();
        auto items = async::publisher<int>{};
        items.observe_on(this_coordinator())
          .do_on_error([err](const error& what) { *err = what; })
          .for_each([this](int) { fail("unexpected value"); });
        run_flows();
        check_eq(*err, sec::invalid_observable);
      }
    }
  }
}

SCENARIO("publishers from default-constructed observables are invalid") {
  GIVEN("publisher with a default-constructed observable") {
    WHEN("an actor subscribes to it") {
      THEN("the actor observes an invalid_observable error") {
        auto err = std::make_shared<error>();
        auto items = async::publisher<int>::from({});
        items.observe_on(this_coordinator())
          .do_on_error([err](const error& what) { *err = what; })
          .for_each([this](int) { fail("unexpected value"); });
        run_flows();
        check_eq(*err, sec::invalid_observable);
      }
    }
  }
}
} // WITH_FIXTURE(test::fixture::flow)
*/

WITH_FIXTURE(test::fixture::deterministic) {

SCENARIO("actors can subscribe to publishers from other actors") {
  GIVEN("three actors") {
    WHEN("creating a publisher on one and subscribing on the others") {
      THEN("the other actors receive the values") {
        using int_vec = std::vector<int>;
        auto v1 = std::make_shared<int_vec>();
        auto v2 = std::make_shared<int_vec>();
        auto items = std::make_shared<async::publisher<int>>();
        sys.spawn([items](caf::event_based_actor* self) {
          *items = self->make_observable().iota(1).take(7).to_publisher();
        });
        auto consumer = [this, items](caf::event_based_actor* self,
                                      std::shared_ptr<int_vec> values) {
          items->observe_on(self)
            .do_on_error([this](const error& what) { //
              fail("error: {}", to_string(what));
            })
            .for_each([values](int x) { values->push_back(x); });
        };
        sys.spawn(consumer, v1);
        sys.spawn(consumer, v2);
        dispatch_messages();
        check_eq(*v1, std::vector({1, 2, 3, 4, 5, 6, 7}));
        check_eq(*v2, std::vector({1, 2, 3, 4, 5, 6, 7}));
      }
    }
  }
}

SCENARIO("publishers from terminated actors produce errors") {
  GIVEN("a publisher from a terminated actor") {
    WHEN("another actors subscribes to it") {
      THEN("the subscriber observe an error") {
        auto items = std::make_shared<async::publisher<int>>();
        sys.spawn([items](caf::event_based_actor* self) {
          *items = self->make_observable().iota(1).take(7).to_publisher();
          self->quit();
        });
        dispatch_messages();
        auto err = std::make_shared<error>();
        sys.spawn([this, items, err](caf::event_based_actor* self) {
          items->observe_on(self)
            .do_on_error([err](const error& what) { *err = what; })
            .for_each([this](int) { fail("unexpected value"); });
        });
        dispatch_messages();
        check_eq(*err, sec::disposed);
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
