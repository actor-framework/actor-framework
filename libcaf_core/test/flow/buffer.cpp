// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.buffer

#include "caf/flow/observable.hpp"

#include "core-test.hpp"

#include "caf/flow/coordinator.hpp"
#include "caf/flow/item_publisher.hpp"
#include "caf/flow/merge.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/scoped_coordinator.hpp"

using namespace caf;
using namespace std::literals;

namespace {

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("the buffer operator groups items together") {
  GIVEN("an observable") {
    WHEN("calling .buffer(3)") {
      THEN("the observer receives values in groups of three") {
        auto inputs = std::vector<int>{1, 2, 4, 8, 16, 32, 64, 128};
        auto outputs = std::vector<cow_vector<int>>{};
        auto expected = std::vector<cow_vector<int>>{
          cow_vector<int>{1, 2, 4},
          cow_vector<int>{8, 16, 32},
          cow_vector<int>{64, 128},
        };
        ctx->make_observable()
          .from_container(inputs) //
          .buffer(3)
          .for_each([&outputs](const cow_vector<int>& xs) {
            outputs.emplace_back(xs);
          });
        ctx->run();
        CHECK_EQ(outputs, expected);
      }
    }
  }
}

SCENARIO("the buffer operator forces items at regular intervals") {
  GIVEN("an observable") {
    WHEN("calling .buffer(3, 1s)") {
      THEN("the observer receives values in groups of three or after 1s") {
        auto outputs = std::make_shared<std::vector<cow_vector<int>>>();
        auto expected = std::vector<cow_vector<int>>{
          cow_vector<int>{1, 2, 4}, cow_vector<int>{8, 16, 32},
          cow_vector<int>{},        cow_vector<int>{64},
          cow_vector<int>{},        cow_vector<int>{128, 256, 512},
        };
        auto pub = flow::item_publisher<int>{ctx.get()};
        sys.spawn([&pub, outputs](caf::event_based_actor* self) {
          pub //
            .as_observable()
            .observe_on(self)
            .buffer(3, 1s)
            .for_each([outputs](const cow_vector<int>& xs) {
              outputs->emplace_back(xs);
            });
        });
        sched.run();
        MESSAGE("emit the first six items");
        pub.push({1, 2, 4, 8, 16, 32});
        ctx->run_some();
        sched.run();
        MESSAGE("force an empty buffer");
        advance_time(1s);
        sched.run();
        MESSAGE("force a buffer with a single element");
        pub.push(64);
        ctx->run_some();
        sched.run();
        advance_time(1s);
        sched.run();
        MESSAGE("force an empty buffer");
        advance_time(1s);
        sched.run();
        MESSAGE("emit the last items and close the source");
        pub.push({128, 256, 512});
        pub.close();
        ctx->run_some();
        sched.run();
        advance_time(1s);
        sched.run();
        CHECK_EQ(*outputs, expected);
      }
    }
  }
}

SCENARIO("the buffer operator forwards errors") {
  GIVEN("an observable that produces some values followed by an error") {
    WHEN("calling .buffer() on it") {
      THEN("the observer receives the values and then the error") {
        auto outputs = std::make_shared<std::vector<cow_vector<int>>>();
        auto err = std::make_shared<error>();
        sys.spawn([outputs, err](caf::event_based_actor* self) {
          auto obs = self->make_observable();
          obs.iota(1)
            .take(17)
            .concat(obs.fail<int>(make_error(caf::sec::runtime_error)))
            .buffer(7, 1s)
            .do_on_error([err](const error& what) { *err = what; })
            .for_each([outputs](const cow_vector<int>& xs) {
              outputs->emplace_back(xs);
            });
        });
        sched.run();
        auto expected = std::vector<cow_vector<int>>{
          cow_vector<int>{1, 2, 3, 4, 5, 6, 7},
          cow_vector<int>{8, 9, 10, 11, 12, 13, 14},
          cow_vector<int>{15, 16, 17},
        };
        CHECK_EQ(*outputs, expected);
        CHECK_EQ(*err, caf::sec::runtime_error);
      }
    }
  }
  GIVEN("an observable that produces only an error") {
    WHEN("calling .buffer() on it") {
      THEN("the observer receives the error") {
        auto outputs = std::make_shared<std::vector<cow_vector<int>>>();
        auto err = std::make_shared<error>();
        sys.spawn([outputs, err](caf::event_based_actor* self) {
          self->make_observable()
            .fail<int>(make_error(caf::sec::runtime_error))
            .buffer(3, 1s)
            .do_on_error([err](const error& what) { *err = what; })
            .for_each([outputs](const cow_vector<int>& xs) {
              outputs->emplace_back(xs);
            });
        });
        sched.run();
        CHECK(outputs->empty());
        CHECK_EQ(*err, caf::sec::runtime_error);
      }
    }
  }
}

END_FIXTURE_SCOPE()
