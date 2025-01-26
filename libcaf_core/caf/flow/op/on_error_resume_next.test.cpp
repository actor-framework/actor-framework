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
  // Similar to on_error_resume_next::subscribe, but returns a
  // on_error_resume_next_sub pointer instead of type-erasing it into a
  // disposable.
  template <class T = int, class Predicate>
  auto raw_sub(caf::flow::observable<int> in, caf::flow::observer<int> out,
               Predicate predicate, caf::flow::observable<int> fallback) {
    using sub_t = caf::flow::op::on_error_resume_next_sub<T, Predicate>;
    auto ptr = make_counted<sub_t>(coordinator(), out, in, std::move(predicate),
                                   std::move(fallback));
    out.on_subscribe(caf::flow::subscription{ptr});
    return ptr;
  }
};

WITH_FIXTURE(fixture) {
SCENARIO("the on_error_resume_next operator resumes observables on error") {
  int retry_count = 0;
  auto on_error_resume_next_predicate = [&retry_count](const caf::error& what) {
    if (what == sec::invalid_request && retry_count < max_retry_count) {
      ++retry_count;
      return true;
    }
    return false;
  };
  GIVEN("an observable that always fails") {
    WHEN(
      "calling on_error_resume_next with an observable that always completes") {
      THEN("the observer completes with fallback") {
        auto outputs = std::make_shared<std::vector<int>>();
        auto err = std::make_shared<error>();
        auto input = std::vector{1, 2, 3, 4};
        auto completed = std::make_shared<bool>(false);
        make_observable()
          .from_container(input)
          .concat(make_observable().fail<int32_t>(sec::invalid_request))
          .on_error_resume_next(
            on_error_resume_next_predicate,
            make_observable().iota(9).take(2).as_observable())
          .do_on_complete([completed]() { *completed = true; })
          .do_on_error([err](const error& what) { *err = what; })
          .for_each([outputs](const int& xs) { outputs->emplace_back(xs); });
        run_flows();
        check_eq(outputs->size(), 6u);
        check_eq(*err, error{}); // no error
        check_eq(*outputs, std::vector<int>{1, 2, 3, 4, 9, 10});
        check(*completed);
        check_eq(retry_count, 1);
      }
    }
  }
  GIVEN("an observable that always fails") {
    WHEN("calling on_error_resume_next with an observable that always fails") {
      THEN("the observer completes with fallback") {
        auto outputs = std::make_shared<std::vector<int>>();
        auto err = std::make_shared<error>();
        auto input = std::vector{1, 2, 3, 4};
        auto completed = std::make_shared<bool>(false);
        make_observable()
          .from_container(input)
          .concat(make_observable().fail<int32_t>(sec::invalid_request))
          .on_error_resume_next(
            on_error_resume_next_predicate,
            make_observable()
              .iota(9)
              .take(2)
              .concat(make_observable().fail<int32_t>(sec::invalid_request))
              .as_observable())
          .do_on_complete([completed]() { *completed = true; })
          .do_on_error([err](const error& what) { *err = what; })
          .for_each([outputs](const int& xs) { outputs->emplace_back(xs); });
        run_flows();
        check_eq(outputs->size(), 10u);
        check_eq(*err, error{sec::invalid_request}); // no error
        check_eq(*outputs, std::vector<int>{1, 2, 3, 4, 9, 10, 9, 10, 9, 10});
        check(!*completed);
        check_eq(retry_count, 3);
      }
    }
  }
  GIVEN("an observable that always completes") {
    WHEN("calling on_error_resume_next") {
      THEN("the observer completes without retrying") {
        auto outputs = std::make_shared<std::vector<int>>();
        auto err = std::make_shared<error>();
        auto completed = std::make_shared<bool>(false);
        auto input = std::vector{1, 2, 3, 4};
        make_observable()
          .from_container(input)
          .on_error_resume_next(
            on_error_resume_next_predicate,
            make_observable().iota(9).take(2).as_observable())
          .do_on_error([err](const error& what) { *err = what; })
          .do_on_complete([completed]() { *completed = true; })
          .for_each([outputs](const int& xs) { outputs->emplace_back(xs); });
        run_flows();
        check_eq(outputs->size(), 4u);
        check_eq(*err, error{}); // no error
        check(*completed);
        check_eq(retry_count, 0);
      }
    }
  }
}

SCENARIO(
  "the on_error_resume_next operator can complete flows despite errors") {
  auto on_error_resume_next_predicate = [](const caf::error& what) {
    return what == sec::runtime_error;
  };
  GIVEN("an observable that is resumed on `runtime_error`") {
    auto snk = flow::make_passive_observer<int>();
    auto uut = raw_sub(make_observable().never<int>(), snk->as_observer(),
                       on_error_resume_next_predicate,
                       make_observable().never<int>());
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

SCENARIO("disposing a on_error_resume_next operator aborts the flow") {
  auto on_error_resume_next_predicate = [](const caf::error& what) {
    return what == sec::runtime_error;
  };
  GIVEN("an observable that is retried on `runtime_error`") {
    WHEN("disposing the subscription of the operator") {
      THEN("the observer receives an on_error event") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(make_observable().never<int>(), snk->as_observer(),
                           on_error_resume_next_predicate,
                           make_observable().never<int>());
        snk->request(42);
        run_flows();
        uut->dispose();
        run_flows();
        check(snk->aborted());
      }
    }
  }
}

SCENARIO(
  "on_error_resume_next operators discard items when there is no demand") {
  GIVEN("an observable that is retried on `runtime_error`") {
    WHEN("the demand is met") {
      THEN("the observer stops receiving new items") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(
          make_observable().never<int>(), snk->as_observer(),
          [](const caf::error&) { return true; },
          make_observable().never<int>());
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

SCENARIO("on_error_resume_next operators forwards existing demand on retry") {
  GIVEN("an observable that is retried on `runtime_error`") {
    WHEN("on_error is called on the operator") {
      THEN("the observable forwards existing demand") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(
          make_observable().never<int>(), snk->as_observer(),
          [](const caf::error&) { return true; },
          make_observable().never<int>());
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
