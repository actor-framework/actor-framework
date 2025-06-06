// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/op/debounce.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/flow/multicaster.hpp"
#include "caf/flow/observable.hpp"
#include "caf/log/test.hpp"

using namespace caf;
using namespace std::literals;

namespace {

struct fixture : test::fixture::deterministic, test::fixture::flow {
  // Similar to debounce::subscribe, but returns a debounce_sub pointer instead
  // of type-erasing it into a disposable.
  template <class T = int>
  auto raw_sub(caf::flow::observable<int> in, caf::flow::observer<int> out) {
    using sub_t = caf::flow::op::debounce_sub<T>;
    auto ptr = make_counted<sub_t>(coordinator(), out, 100ms);
    ptr->init(in);
    out.on_subscribe(caf::flow::subscription{ptr});
    return ptr;
  }

  template <class T>
  auto make_never_sub(caf::flow::observer<T> out) {
    return make_counted<caf::flow::op::never_sub<T>>(coordinator(), out);
  }
};

WITH_FIXTURE(fixture) {

SCENARIO("the debounce operator emits items at regular intervals") {
  GIVEN("an observable") {
    WHEN("calling .debounce(100ms)") {
      THEN("the observer receives most recent values after 1s") {
        auto outputs = std::make_shared<std::vector<int>>();
        auto expected = std::vector<int>{4, 64, 512};
        auto closed = std::make_shared<bool>(false);
        auto pub = caf::flow::multicaster<int>{coordinator()};
        sys.spawn([&pub, outputs, closed](caf::event_based_actor* self) {
          pub.as_observable()
            .observe_on(self) //
            .debounce(100ms)
            .do_on_complete([closed] { *closed = true; })
            .for_each([outputs](int val) { outputs->emplace_back(val); });
        });
        dispatch_messages();
        log::test::debug("pushing 1, 2, 4 at once with delay of 300ms");
        pub.push({1, 2, 4});
        run_flows();
        dispatch_messages();
        advance_time(300ms);
        run_flows();
        dispatch_messages();
        check_eq(*outputs, std::vector<int>{4});
        log::test::debug("pushing 8, 16 at once with delay of 50ms");
        pub.push({8, 16});
        run_flows();
        dispatch_messages();
        advance_time(50ms);
        run_flows();
        dispatch_messages();
        log::test::debug("pushing 32 with delay of 80ms");
        pub.push({32});
        run_flows();
        dispatch_messages();
        advance_time(80ms);
        run_flows();
        dispatch_messages();
        log::test::debug("pushing 64 with delay of 80ms");
        pub.push({64});
        run_flows();
        dispatch_messages();
        advance_time(100ms);
        run_flows();
        dispatch_messages();
        log::test::debug(
          "pushing 128, 256, 512 with delay of 60ms and closing pub");
        pub.push({128, 256, 512});
        run_flows();
        dispatch_messages();
        advance_time(60ms);
        run_flows();
        dispatch_messages();
        pub.close();
        run_flows();
        dispatch_messages();
        check_eq(*outputs, expected);
        check(*closed);
      }
    }
  }
}

SCENARIO("the debounce operator forwards errors") {
  GIVEN("an observable that produces some values followed by an error") {
    WHEN("calling .debounce() on it") {
      THEN("the observer receives the values and then the error") {
        auto outputs = std::make_shared<std::vector<int>>();
        auto err = std::make_shared<error>();
        auto pub = caf::flow::multicaster<int>{coordinator()};
        sys.spawn([&pub, outputs, err](caf::event_based_actor* self) {
          pub.as_observable()
            .observe_on(self) //
            .concat(self->make_observable().fail<int>(
              make_error(caf::sec::runtime_error)))
            .debounce(100ms)
            .do_on_error([err](const error& what) { *err = what; })
            .for_each([outputs](const int& xs) { outputs->emplace_back(xs); });
        });
        dispatch_messages();
        pub.push({1});
        run_flows();
        dispatch_messages();
        advance_time(100ms);
        dispatch_messages();
        pub.push({2, 3});
        run_flows();
        dispatch_messages();
        advance_time(100ms);
        dispatch_messages();
        pub.push({4});
        run_flows();
        dispatch_messages();
        advance_time(100ms);
        dispatch_messages();
        pub.close();
        run_flows();
        advance_time(100ms);
        dispatch_messages();
        auto expected = std::vector<int>{1, 3, 4};
        check_eq(*outputs, expected);
        check_eq(*err, caf::sec::runtime_error);
      }
    }
  }
  GIVEN("an observable that produces only an error") {
    WHEN("calling .debounce() on it") {
      THEN("the observer receives the error") {
        auto outputs = std::make_shared<std::vector<int>>();
        auto err = std::make_shared<error>();
        sys.spawn([outputs, err](caf::event_based_actor* self) {
          self->make_observable()
            .fail<int>(make_error(caf::sec::runtime_error))
            .debounce(100ms)
            .do_on_error([err](const error& what) { *err = what; })
            .for_each([outputs](const int& xs) { outputs->emplace_back(xs); });
        });
        run_flows();
        dispatch_messages();
        advance_time(100ms);
        dispatch_messages();
        check(outputs->empty());
        check_eq(*err, caf::sec::runtime_error);
      }
    }
  }
}

SCENARIO("debounces dispose unexpected subscriptions") {
  GIVEN("an initialized debounce operator") {
    WHEN("calling on_subscribe with unexpected subscriptions") {
      THEN("the debounce disposes them immediately") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(make_observable().never<int>(), snk->as_observer());
        auto data_sub = make_never_sub<int>(snk->as_observer());
        uut->on_subscribe(caf::flow::subscription{data_sub});
        check(snk->subscribed());
        check(!uut->disposed());
        run_flows();
        check(data_sub->disposed());
        uut->dispose();
        run_flows();
        check(uut->disposed());
      }
    }
  }
}

SCENARIO("debounces cancels unexpected subscriptions") {
  GIVEN("an initialized debounce operator") {
    WHEN("calling on_subscribe with unexpected subscriptions") {
      THEN("the debounce cancels them immediately") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(make_observable().never<int>(), snk->as_observer());
        auto data_sub = make_never_sub<int>(snk->as_observer());
        uut->on_subscribe(caf::flow::subscription{data_sub});
        check(snk->subscribed());
        check(!uut->disposed());
        run_flows();
        check(data_sub->disposed());
        uut->cancel();
        run_flows();
        check(uut->disposed());
      }
    }
  }
}

SCENARIO("debounces emit final items after an on_error event") {
  GIVEN("an initialized debounce operator") {
    WHEN("calling on_error on a debounce without pending data") {
      THEN("the debounce forward on_error immediately") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(make_observable().never<int>(), snk->as_observer());
        snk->request(42);
        run_flows();
        check_eq(uut->pending(), false);
        uut->on_error(sec::runtime_error);
        check(snk->aborted());
      }
    }
    WHEN("calling on_error on a debounce with pending data") {
      THEN("the debounce still emits pending data before closing") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(make_observable().never<int>(), snk->as_observer());
        snk->request(42);
        uut->on_next(1);
        uut->on_next(2);
        check_eq(uut->pending(), true);
        check(snk->buf.empty());
        uut->on_error(sec::runtime_error);
        check(snk->aborted());
        uut->dispose();
        check_eq(snk->buf, std::vector<int>{2});
        check(snk->aborted());
      }
    }
  }
}

SCENARIO("debounces emit final items after an on_complete event") {
  GIVEN("an initialized debounce operator") {
    WHEN("calling on_complete on a debounce without pending data") {
      THEN("the debounce forward on_complete immediately") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(make_observable().never<int>(), snk->as_observer());
        snk->request(42);
        uut->on_next(1);
        uut->on_next(2);
        uut->on_next(3);
        advance_time(100ms);
        dispatch_messages();
        run_flows();
        check_eq(uut->pending(), false);
        uut->on_complete();
        check_eq(snk->buf, std::vector<int>{3});
        check(snk->completed());
      }
    }
    WHEN("calling on_complete on a debounce with pending data") {
      THEN("the debounce still emits pending data before closing") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(make_observable().never<int>(), snk->as_observer());
        snk->request(42);
        uut->on_next(1);
        uut->on_next(2);
        check_eq(uut->pending(), true);
        check(snk->buf.empty());
        check(!snk->completed());
        uut->on_complete();
        run_flows();
        check_eq(snk->buf, std::vector<int>{2});
        check(snk->completed());
      }
    }
  }
}

SCENARIO("disposing a debounce operator completes the flow") {
  GIVEN("a debounce operator") {
    WHEN("disposing the subscription operator of the operator") {
      THEN("the observer receives an on_complete event") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(make_observable().never<int>(), snk->as_observer());
        snk->request(42);
        run_flows();
        uut->dispose();
        run_flows();
        check(snk->aborted());
      }
    }
  }
}

} // WITH_FIXTURE(fixture)

} // namespace
