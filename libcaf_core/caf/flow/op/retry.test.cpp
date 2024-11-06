// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/log/test.hpp"

using namespace caf;

namespace {

constexpr int max_retry_count = 3;

struct fixture : test::fixture::deterministic, test::fixture::flow {
  // Similar to retry::subscribe, but returns a retry_sub pointer instead of
  // type-erasing it into a disposable.
  template <class T = int, class Predicate>
  auto raw_sub(caf::flow::observable<int> in, caf::flow::observer<int> out,
               Predicate predicate) {
    using sub_t = caf::flow::op::retry_sub<T, Predicate>;
    auto ptr = make_counted<sub_t>(coordinator(), out, in,
                                   std::move(predicate));
    out.on_subscribe(caf::flow::subscription{ptr});
    return ptr;
  }
};

WITH_FIXTURE(fixture) {

SCENARIO("the retry operator re-subscribes observables on error") {
  int retry_count = 0;
  auto retry_predicate = [&retry_count](const caf::error& what) {
    if (what == sec::invalid_request && retry_count < max_retry_count) {
      ++retry_count;
      return true;
    }
    return false;
  };
  GIVEN("an observable that always fails") {
    WHEN("calling retry") {
      THEN("the observer receives an error after retrying 3 times") {
        auto outputs = std::make_shared<std::vector<int>>();
        auto err = std::make_shared<error>();
        auto input = std::vector{1, 2, 3, 4};
        make_observable()
          .from_container(input)
          .concat(make_observable().fail<int32_t>(sec::invalid_request))
          .retry(retry_predicate)
          .do_on_error([err](const error& what) { *err = what; })
          .for_each([outputs](const int& xs) { outputs->emplace_back(xs); });
        run_flows();
        check_eq(outputs->size(), 16u);
        check_eq(retry_count, 3);
        check_eq(*err, error{sec::invalid_request});
      }
    }
  }
  GIVEN("an observable that always completes") {
    WHEN("calling retry") {
      THEN("the observer completes without retrying") {
        auto outputs = std::make_shared<std::vector<int>>();
        auto err = std::make_shared<error>();
        auto completed = std::make_shared<bool>(false);
        auto input = std::vector{1, 2, 3, 4};
        make_observable()
          .from_container(input)
          .retry(retry_predicate)
          .do_on_error([err](const error& what) { *err = what; })
          .do_on_complete([completed]() { *completed = true; })
          .for_each([outputs](const int& xs) { outputs->emplace_back(xs); });
        run_flows();
        check_eq(outputs->size(), 4u);
        check_eq(retry_count, 0);
        check_eq(*err, error{}); // no error
        check(*completed);
      }
    }
  }
}

SCENARIO("the retry operator can complete flows despite errors") {
  auto retry_predicate = [](const caf::error& what) {
    return what == sec::runtime_error;
  };
  GIVEN("an observable that is retried on `runtime_error`") {
    auto snk = flow::make_passive_observer<int>();
    auto uut = raw_sub(make_observable().never<int>(), snk->as_observer(),
                       retry_predicate);
    snk->request(42);
    run_flows();
    WHEN("calling on_error with runtime_error once") {
      THEN("the flow completes without error") {
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
    WHEN("calling on_error with runtime_error twice") {
      THEN("the flow completes without error") {
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
    WHEN("calling on_error on operator with unexpected_message") {
      THEN("the flow aborts with an error") {
        uut->on_next(1);
        uut->on_next(2);
        run_flows();
        uut->on_error(sec::unexpected_message);
        run_flows();
        check_eq(snk->buf, std::vector<int>{1, 2});
        check(snk->aborted());
      }
    }
    WHEN("calling on_error on operator after the flow is completed") {
      THEN("the items are not emitted") {
        uut->on_next(1);
        run_flows();
        uut->on_complete();
        run_flows();
        check_eq(snk->buf, std::vector<int>{1});
        check(snk->completed());
        uut->on_next(3);
        run_flows();
        check_eq(snk->buf, std::vector<int>{1});
      }
    }
    WHEN("calling on_error on operator after the flow is aborted") {
      THEN("the items are not emitted") {
        uut->on_next(1);
        run_flows();
        uut->on_error(sec::unexpected_message);
        run_flows();
        check_eq(snk->buf, std::vector<int>{1});
        check(snk->aborted());
        uut->on_next(3);
        run_flows();
        check_eq(snk->buf, std::vector<int>{1});
      }
    }
  }
}

SCENARIO("disposing a retry operator aborts the flow") {
  auto retry_predicate = [](const caf::error& what) {
    return what == sec::runtime_error;
  };
  GIVEN("an observable that is retried on `runtime_error`") {
    WHEN("disposing the subscription of the operator") {
      THEN("the observer receives an on_error event") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(make_observable().never<int>(), snk->as_observer(),
                           retry_predicate);
        snk->request(42);
        run_flows();
        uut->dispose();
        run_flows();
        check(snk->aborted());
      }
    }
  }
}

SCENARIO("retry operators discard items when there is no demand") {
  GIVEN("an observable that is retried on `runtime_error`") {
    WHEN("the demand is met") {
      THEN("the observer stops receiving new items") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(make_observable().never<int>(), snk->as_observer(),
                           [](const caf::error&) { return true; });
        snk->request(2);
        uut->on_next(1);
        uut->on_next(2);
        uut->on_next(3);
        uut->on_complete();
        run_flows();
        check_eq(snk->buf, std::vector<int>{1, 2});
        check(snk->completed());
      }
    }
  }
}

SCENARIO("retry operators forwards existing demand on retry") {
  GIVEN("an observable that is retried on `runtime_error`") {
    WHEN("on_error is called on the operator") {
      THEN("the observable forwards existing demand") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(make_observable().never<int>(), snk->as_observer(),
                           [](const caf::error&) { return true; });
        snk->request(4);
        uut->on_next(1);
        uut->on_next(2);
        run_flows();
        uut->on_error(sec::runtime_error);
        uut->on_next(3);
        uut->on_next(4);
        uut->on_next(5);
        run_flows();
        check_eq(snk->buf, std::vector<int>{1, 2, 3, 4});
        snk->request(2);
        uut->on_next(6);
        uut->on_complete();
        run_flows();
        check_eq(snk->buf, std::vector<int>{1, 2, 3, 4, 6});
        check(snk->completed());
      }
    }
  }
}

} // WITH_FIXTURE(fixture)

} // namespace
