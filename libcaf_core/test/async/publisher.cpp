// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE async.publisher

#include "caf/async/publisher.hpp"

#include "caf/flow/scoped_coordinator.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include "core-test.hpp"

using namespace caf;

BEGIN_FIXTURE_SCOPE(test_coordinator_fixture<>)

SCENARIO("actors can subscribe to their own publishers") {
  GIVEN("an observable") {
    WHEN("converting it to a publisher") {
      THEN("the owning actor can subscribe to it") {
        auto values = std::make_shared<std::vector<int>>();
        sys.spawn([values](caf::event_based_actor* self) {
          self->make_observable()
            .iota(1)
            .take(7)
            .to_publisher()
            .observe_on(self)
            .do_on_error([](const error& what) { FAIL("error: " << what); })
            .for_each([values](int x) { values->push_back(x); });
        });
        run();
        CHECK_EQ(*values, std::vector({1, 2, 3, 4, 5, 6, 7}));
      }
    }
  }
}

SCENARIO("default-constructed publishers are invalid") {
  GIVEN("a default-constructed publisher") {
    WHEN("an actor subscribes to it") {
      THEN("the actor observes an invalid_observable error") {
        auto err = std::make_shared<error>();
        sys.spawn([err](caf::event_based_actor* self) {
          auto items = async::publisher<int>{};
          items.observe_on(self)
            .do_on_error([err](const error& what) { *err = what; })
            .for_each([](int) { FAIL("unexpected value"); });
        });
        run();
        CHECK_EQ(*err, sec::invalid_observable);
      }
    }
  }
}

SCENARIO("publishers from default-constructed observables are invalid") {
  GIVEN("publisher with a default-constructed observable") {
    WHEN("an actor subscribes to it") {
      THEN("the actor observes an invalid_observable error") {
        auto err = std::make_shared<error>();
        sys.spawn([err](caf::event_based_actor* self) {
          auto items = async::publisher<int>::from({});
          items.observe_on(self)
            .do_on_error([err](const error& what) { *err = what; })
            .for_each([](int) { FAIL("unexpected value"); });
        });
        run();
        CHECK_EQ(*err, sec::invalid_observable);
      }
    }
  }
}

SCENARIO("actors can subscribe to publishers from other actors") {
  GIVEN("three actors") {
    WHEN("creating a publisher on one and subscribing on the others") {
      THEN("the other actors receive the values") {
        auto v1 = std::make_shared<std::vector<int>>();
        auto v2 = std::make_shared<std::vector<int>>();
        auto items = std::make_shared<async::publisher<int>>();
        sys.spawn([items](caf::event_based_actor* self) {
          *items = self->make_observable().iota(1).take(7).to_publisher();
        });
        run();
        auto consumer = [items](caf::event_based_actor* self,
                                std::shared_ptr<std::vector<int>> values) {
          items->observe_on(self)
            .do_on_error([](const error& what) { FAIL("error: " << what); })
            .for_each([values](int x) { values->push_back(x); });
        };
        sys.spawn(consumer, v1);
        sys.spawn(consumer, v2);
        run();
        CHECK_EQ(*v1, std::vector({1, 2, 3, 4, 5, 6, 7}));
        CHECK_EQ(*v2, std::vector({1, 2, 3, 4, 5, 6, 7}));
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
        run();
        auto err = std::make_shared<error>();
        sys.spawn([items, err](caf::event_based_actor* self) {
          items->observe_on(self)
            .do_on_error([err](const error& what) { *err = what; })
            .for_each([](int) { FAIL("unexpected value"); });
        });
        run();
        CHECK_EQ(*err, sec::disposed);
      }
    }
  }
}

END_FIXTURE_SCOPE()
