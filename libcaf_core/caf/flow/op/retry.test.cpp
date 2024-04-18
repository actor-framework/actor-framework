// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/flow/multicaster.hpp"
#include "caf/flow/observable.hpp"
#include "caf/log/test.hpp"

using namespace caf;

namespace {

struct fixture : test::fixture::deterministic, test::fixture::flow {
  // Similar to retry::subscribe, but returns a retry_sub pointer instead of
  // type-erasing it into a disposable.
  template <class T = int>
  auto raw_sub(caf::flow::observable<int> in, caf::flow::observer<int> out) {
    using sub_t = caf::flow::op::retry_sub<T>;
    auto ptr = make_counted<sub_t>(coordinator(), out, 3, in);
    out.on_subscribe(caf::flow::subscription{ptr});
    return ptr;
  }
};

WITH_FIXTURE(fixture) {

SCENARIO("the retry operator retries observable multiple times") {
  GIVEN("an observable that always fails") {
    WHEN("calling retry(3)") {
      THEN("the observer receives error after retrying 3 times") {
        auto outputs = std::make_shared<std::vector<int>>();
        auto err = std::make_shared<error>();
        auto input = std::vector{1, 2, 3, 4};
        make_observable()
          .from_container(input)
          .concat(make_observable().fail<int32_t>(sec::invalid_request))
          .retry(3)
          .do_on_error([err](const error& what) { *err = what; })
          .for_each([outputs](const int& xs) { outputs->emplace_back(xs); });
        run_flows();
        check_eq(outputs->size(), 16u);
        check_eq(*err, error{sec::invalid_request});
      }
    }
  }
  GIVEN("an observable that always completes") {
    WHEN("calling retry(3)") {
      THEN("the observer completes without retrying") {
        auto outputs = std::make_shared<std::vector<int>>();
        auto err = std::make_shared<error>();
        auto completed = std::make_shared<bool>(false);
        auto input = std::vector{1, 2, 3, 4};
        make_observable()
          .from_container(input)
          .retry(3)
          .do_on_error([err](const error& what) { *err = what; })
          .do_on_complete([completed]() { *completed = true; })
          .for_each([outputs](const int& xs) { outputs->emplace_back(xs); });
        run_flows();
        check_eq(outputs->size(), 4u);
        check_eq(*err, error{}); // no error
        check(*completed);
      }
    }
  }
}

SCENARIO("retry can complete flow despite multiple errors") {
  GIVEN("an initialized retry operator") {
    auto snk = flow::make_passive_observer<int>();
    auto uut = raw_sub(make_observable().never<int>(), snk->as_observer());
    snk->request(42);
    run_flows();
    WHEN("calling on_error on operator once") {
      THEN("flow is completed without forwarding error") {
        uut->on_next(1);
        uut->on_next(2);
        run_flows();
        uut->on_error(sec::runtime_error);
        run_flows();
        uut->on_next(1);
        uut->on_complete();
        run_flows();
        check_eq(snk->buf, std::vector<int>{1, 2, 1});
        check(snk->completed());
      }
    }
    WHEN("calling on_error on operator twice") {
      THEN("flow is completed without forwarding error") {
        uut->on_next(1);
        uut->on_next(2);
        run_flows();
        uut->on_error(sec::runtime_error);
        run_flows();
        uut->on_next(1);
        uut->on_error(sec::runtime_error);
        run_flows();
        uut->on_next(1);
        uut->on_next(2);
        uut->on_complete();
        run_flows();
        check_eq(snk->buf, std::vector<int>{1, 2, 1, 1, 2});
        check(snk->completed());
      }
    }
  }
}

SCENARIO("disposing a retry operator completes the flow") {
  GIVEN("a retry operator") {
    WHEN("disposing the subscription of the operator") {
      THEN("the observer receives an on_error event") {
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
