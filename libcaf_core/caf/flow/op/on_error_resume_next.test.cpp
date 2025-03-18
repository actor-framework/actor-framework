// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/flow/multicaster.hpp"
#include "caf/log/test.hpp"

using namespace caf;

namespace {

WITH_FIXTURE(test::fixture::flow) {

SCENARIO("on_error_resume_next switches to a fallback observable on error") {
  auto predicate = [](const caf::error& what) {
    return what == sec::invalid_request;
  };
  GIVEN("an observable that fails with sec::invalid_request") {
    WHEN("calling on_error_resume_next with a fallback that completes") {
      THEN("the observer completes with the fallback") {
        auto outputs = std::make_shared<std::vector<int>>();
        auto err = std::make_shared<error>();
        auto input = std::vector{1, 2, 3, 4};
        auto completed = std::make_shared<bool>(false);
        make_observable()
          .from_container(input)
          .concat(make_observable().fail<int>(sec::invalid_request))
          .on_error_resume_next(predicate, make_observable().iota(9).take(2))
          .do_on_complete([completed]() { *completed = true; })
          .do_on_error([err](const error& what) { *err = what; })
          .for_each([outputs](const int& xs) { outputs->emplace_back(xs); });
        run_flows();
        check_eq(*err, error{}); // no error
        check_eq(*outputs, std::vector<int>{1, 2, 3, 4, 9, 10});
        check(*completed);
      }
    }
    WHEN("calling on_error_resume_next with a fallback that fails") {
      THEN("the observer completes with the error") {
        auto outputs = std::make_shared<std::vector<int>>();
        auto err = std::make_shared<error>();
        auto input = std::vector{1, 2, 3, 4};
        auto completed = std::make_shared<bool>(false);
        make_observable()
          .from_container(input)
          .concat(make_observable().fail<int>(sec::invalid_request))
          .on_error_resume_next(
            predicate, make_observable().iota(9).take(2).concat(
                         make_observable().fail<int>(sec::invalid_request)))
          .do_on_complete([completed]() { *completed = true; })
          .do_on_error([err](const error& what) { *err = what; })
          .for_each([outputs](const int& xs) { outputs->emplace_back(xs); });
        run_flows();
        check_eq(*err, error{sec::invalid_request});
        check_eq(*outputs, std::vector<int>{1, 2, 3, 4, 9, 10});
        check(!*completed);
      }
    }
  }
  GIVEN("an observable that fails with sec::runtime_error") {
    WHEN("calling on_error_resume_next with a predicate that does not match") {
      THEN("the observer completes with error without using the fallback") {
        auto outputs = std::make_shared<std::vector<int>>();
        auto err = std::make_shared<error>();
        auto input = std::vector{1, 2, 3, 4};
        auto completed = std::make_shared<bool>(false);
        make_observable()
          .from_container(input)
          .concat(make_observable().fail<int32_t>(sec::runtime_error))
          .on_error_resume_next(predicate, make_observable().iota(9).take(2))
          .do_on_complete([completed]() { *completed = true; })
          .do_on_error([err](const error& what) { *err = what; })
          .for_each([outputs](const int& xs) { outputs->emplace_back(xs); });
        run_flows();
        check_eq(outputs->size(), 4u);
        check_eq(*err, error{sec::runtime_error});
        check(!*completed);
      }
    }
  }
  GIVEN("an observable that completes") {
    WHEN("calling on_error_resume_next") {
      THEN("the observer completes with the original observable") {
        auto outputs = std::make_shared<std::vector<int>>();
        auto err = std::make_shared<error>();
        auto completed = std::make_shared<bool>(false);
        auto input = std::vector{1, 2, 3, 4};
        make_observable()
          .from_container(input)
          .on_error_resume_next(predicate, make_observable().iota(9).take(2))
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

SCENARIO("on_error_resume_next discards items when there is no demand") {
  GIVEN("an observable that is retried on `runtime_error`") {
    WHEN("the demand is met") {
      THEN("the observer stops receiving new items") {
        auto src1 = caf::flow::multicaster<int>(coordinator());
        auto snk = make_passive_observer<int>();
        auto on_error_resume_next_predicate = [](const caf::error& what) {
          return what == sec::runtime_error;
        };
        auto uut = src1.as_observable()
                     .on_error_resume_next(
                       on_error_resume_next_predicate,
                       make_observable().iota(1).take(2).as_observable())
                     .subscribe(snk->as_observer());
        snk->request(2);
        check(src1.push(1));
        check(src1.push(2));
        check(!src1.push(3));
        src1.close();
        run_flows();
        check_eq(snk->buf, std::vector<int>{1, 2});
        snk->request(3);
        run_flows();
        check_eq(snk->buf, std::vector<int>{1, 2, 3});
        check(snk->completed());
      }
    }
  }
}

SCENARIO("on_error_resume_next operators forwards existing demand on retry") {
  GIVEN("an observable that is retried on `runtime_error`") {
    WHEN("an observable is aborted with `runtime_error`") {
      THEN("the observable forwards existing demand") {
        auto src1 = caf::flow::multicaster<int>(coordinator());
        auto src2 = caf::flow::multicaster<int>(coordinator());
        auto snk = make_passive_observer<int>();
        auto on_error_resume_next_predicate = [](const caf::error& what) {
          return what == sec::runtime_error;
        };
        auto uut = src1.as_observable()
                     .on_error_resume_next(on_error_resume_next_predicate,
                                           src2.as_observable())
                     .subscribe(snk->as_observer());
        snk->request(4);
        check_eq(src1.demand(), 4u);
        src1.push(1);
        src1.push(2);
        run_flows();
        check_eq(src1.demand(), 2u);
        src1.abort(sec::runtime_error);
        run_flows();
        check(!snk->completed());
        check_eq(src2.demand(), 2u);
        src2.push(3);
        src2.push(4);
        src2.push(5);
        run_flows();
        check_eq(snk->buf, std::vector<int>{1, 2, 3, 4});
        snk->request(2);
        src2.push(6);
        src2.close();
        run_flows();
        check_eq(snk->buf, std::vector<int>{1, 2, 3, 4, 5, 6});
        check(snk->completed());
      }
    }
  }
}

SCENARIO("disposing a on_error_resume_next operator aborts the flow") {
  GIVEN("an observable that is retried on `runtime_error`") {
    WHEN("disposing the subscription of the operator") {
      THEN("the observer receives an on_error event") {
        auto snk = make_passive_observer<int>();
        auto on_error_resume_next_predicate = [](const caf::error& what) {
          return what == sec::runtime_error;
        };
        auto uut = make_observable()
                     .never<int>()
                     .on_error_resume_next(on_error_resume_next_predicate,
                                           make_observable().never<int>())
                     .subscribe(snk->as_observer());
        snk->request(42);
        run_flows();
        uut.dispose();
        run_flows();
        check(uut.disposed());
        check(snk->aborted());
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::flow)

} // namespace
