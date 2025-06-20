// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/flow/op/throttle_first.hpp"

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

constexpr auto fwd_data = caf::flow::op::throttle_first_input_t{};

constexpr auto fwd_ctrl = caf::flow::op::throttle_first_emit_t{};

struct fixture : test::fixture::deterministic, test::fixture::flow {
  // Similar to throttle_first::subscribe, but returns a throttle_first_sub
  // pointer instead of type-erasing it into a disposable.
  template <class T = int>
  auto raw_sub(caf::flow::observable<int> in,
               caf::flow::observable<int64_t> select,
               caf::flow::observer<int> out) {
    using sub_t = caf::flow::op::throttle_first_sub<T>;
    auto ptr = make_counted<sub_t>(coordinator(), out);
    ptr->init(in, select);
    out.on_subscribe(caf::flow::subscription{ptr});
    return ptr;
  }

  template <class T>
  auto make_never_sub(caf::flow::observer<T> out) {
    return make_counted<caf::flow::op::never_sub<T>>(coordinator(), out);
  }
};

} // namespace

WITH_FIXTURE(fixture) {

SCENARIO("the throttle_first operator emits items at regular intervals") {
  GIVEN("an observable") {
    WHEN("calling .throttle_first(1s)") {
      THEN("the observer receives first emitted value in 1s period") {
        auto outputs = std::make_shared<std::vector<int>>();
        auto expected = std::vector<int>{1, 64, 128};
        auto closed = std::make_shared<bool>(false);
        auto pub = caf::flow::multicaster<int>{coordinator()};
        sys.spawn([&pub, outputs, closed](caf::event_based_actor* self) {
          pub.as_observable()
            .observe_on(self) //
            .throttle_first(1s)
            .do_on_complete([closed] { *closed = true; })
            .for_each([outputs](const int& xs) { outputs->emplace_back(xs); });
        });
        dispatch_messages();
        log::test::debug("emit the first six items");
        pub.push({1, 2, 4, 8, 16, 32});
        run_flows();
        dispatch_messages();
        log::test::debug("force a throttle_first that emits single element");
        advance_time(1s);
        dispatch_messages();
        pub.push(64);
        run_flows();
        dispatch_messages();
        advance_time(1s);
        dispatch_messages();
        log::test::debug("force a throttle_first that does not emit element");
        pub.push({128, 256, 512});
        run_flows();
        advance_time(1s);
        dispatch_messages();
        pub.close();
        run_flows();
        advance_time(1s);
        dispatch_messages();
        check_eq(*outputs, expected);
        check(*closed);
      }
    }
  }
}

SCENARIO("the throttle_first operator forwards errors") {
  GIVEN("an observable that produces some values followed by an error") {
    WHEN("calling .throttle_first() on it") {
      THEN("the observer receives the values and then the error") {
        auto outputs = std::make_shared<std::vector<int>>();
        auto err = std::make_shared<error>();
        auto pub = caf::flow::multicaster<int>{coordinator()};
        sys.spawn([&pub, outputs, err](caf::event_based_actor* self) {
          pub.as_observable()
            .observe_on(self) //
            .concat(self->make_observable().fail<int>(
              make_error(caf::sec::runtime_error)))
            .throttle_first(1s)
            .do_on_error([err](const error& what) { *err = what; })
            .for_each([outputs](const int& xs) { outputs->emplace_back(xs); });
        });
        dispatch_messages();
        pub.push({1});
        run_flows();
        dispatch_messages();
        advance_time(1s);
        dispatch_messages();
        pub.push({2});
        advance_time(1s);
        run_flows();
        dispatch_messages();
        pub.push({3});
        advance_time(1s);
        run_flows();
        dispatch_messages();
        pub.close();
        run_flows();
        advance_time(1s);
        dispatch_messages();
        auto expected = std::vector<int>{1, 2, 3};
        check_eq(*outputs, expected);
        check_eq(*err, caf::sec::runtime_error);
      }
    }
  }
  GIVEN("an observable that produces only an error") {
    WHEN("calling .throttle_first() on it") {
      THEN("the observer receives the error") {
        auto outputs = std::make_shared<std::vector<int>>();
        auto err = std::make_shared<error>();
        sys.spawn([outputs, err](caf::event_based_actor* self) {
          self->make_observable()
            .fail<int>(make_error(caf::sec::runtime_error))
            .throttle_first(1s)
            .do_on_error([err](const error& what) { *err = what; })
            .for_each([outputs](const int& xs) { outputs->emplace_back(xs); });
        });
        run_flows();
        advance_time(1s);
        dispatch_messages();
        check(outputs->empty());
        check_eq(*err, caf::sec::runtime_error);
      }
    }
  }
}

SCENARIO("throttle_first disposes unexpected subscriptions") {
  GIVEN("an initialized throttle_first operator") {
    WHEN("calling on_subscribe with unexpected subscriptions") {
      THEN("throttle_first disposes them immediately") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(make_observable().never<int>(),
                           make_observable().never<int64_t>(),
                           snk->as_observer());
        auto data_sub = make_never_sub<int>(snk->as_observer());
        auto ctrl_sub = make_never_sub<int>(snk->as_observer());
        uut->fwd_on_subscribe(fwd_data, caf::flow::subscription{data_sub});
        uut->fwd_on_subscribe(fwd_ctrl, caf::flow::subscription{ctrl_sub});
        check(snk->subscribed());
        check(!uut->disposed());
        run_flows();
        check(data_sub->disposed());
        check(ctrl_sub->disposed());
        uut->dispose();
        run_flows();
        check(uut->disposed());
      }
    }
  }
}

SCENARIO("throttle_first emit first items after an on_error event") {
  GIVEN("an initialized throttle_first operator") {
    WHEN("calling on_error(data) on a throttle_first without pending data") {
      THEN("the throttle_first forward on_error immediately") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(make_observable().never<int>(),
                           make_observable().never<int64_t>(),
                           snk->as_observer());
        snk->request(42);
        run_flows();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        uut->fwd_on_next(fwd_data, 3);
        uut->fwd_on_next(fwd_ctrl, 1);
        check_eq(uut->pending(), false);
        uut->fwd_on_error(fwd_data, sec::runtime_error);
        uut->fwd_on_next(fwd_ctrl, 1);
        check_eq(snk->buf, std::vector<int>{1});
        check(snk->aborted());
      }
    }
    WHEN("calling on_error(data) on a throttle_first with pending data") {
      THEN("the throttle_first still emits pending data before closing") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(make_observable().never<int>(),
                           make_observable().never<int64_t>(),
                           snk->as_observer());
        run_flows();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        check_eq(uut->pending(), true);
        uut->fwd_on_error(fwd_data, sec::runtime_error);
        check(snk->buf.empty());
        check(!snk->aborted());
        uut->fwd_on_next(fwd_ctrl, 1);
        snk->request(42);
        uut->dispose();
        run_flows();
        check_eq(snk->buf, std::vector<int>{});
        check(snk->aborted());
      }
    }
    WHEN("calling on_error(control) on a throttle_first without pending data") {
      THEN("the throttle_first forward on_error immediately") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(make_observable().never<int>(),
                           make_observable().never<int64_t>(),
                           snk->as_observer());
        snk->request(42);
        run_flows();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        uut->fwd_on_next(fwd_data, 3);
        uut->fwd_on_next(fwd_ctrl, 1);
        check_eq(uut->pending(), false);
        uut->fwd_on_error(fwd_ctrl, sec::runtime_error);
        check_eq(snk->buf, std::vector<int>{1});
        check(snk->aborted());
        uut->dispose();
        run_flows();
      }
    }
    WHEN("calling on_error(control) on a throttle_first with pending data") {
      THEN("throttle_first still emits pending data before closing") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(make_observable().never<int>(),
                           make_observable().never<int64_t>(),
                           snk->as_observer());
        run_flows();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        check_eq(uut->pending(), true);
        check(!snk->aborted());
        uut->fwd_on_error(fwd_ctrl, sec::runtime_error);
        check(snk->buf.empty());
        check(snk->aborted());
        uut->dispose();
        run_flows();
        check_eq(snk->buf, std::vector<int>{});
      }
    }
  }
}

SCENARIO("throttle_firsts emit final items after an on_complete event") {
  GIVEN("an initialized throttle_first operator") {
    WHEN("calling on_complete(data) on a throttle_first without pending data") {
      THEN("the throttle_first forward on_complete immediately") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(make_observable().never<int>(),
                           make_observable().never<int64_t>(),
                           snk->as_observer());
        snk->request(42);
        run_flows();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        uut->fwd_on_next(fwd_data, 3);
        uut->fwd_on_next(fwd_ctrl, 1);
        check_eq(uut->pending(), false);
        uut->fwd_on_complete(fwd_data);
        uut->fwd_on_next(fwd_ctrl, 1);
        check_eq(snk->buf, std::vector<int>{1});
        check(snk->completed());
      }
    }
    WHEN("calling on_complete(data) on a throttle_first with pending data") {
      THEN("the throttle_first still emits pending data before closing") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(make_observable().never<int>(),
                           make_observable().never<int64_t>(),
                           snk->as_observer());
        run_flows();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        check_eq(uut->pending(), true);
        uut->fwd_on_complete(fwd_data);
        check(snk->buf.empty());
        check(!snk->completed());
        uut->fwd_on_next(fwd_ctrl, 1);
        run_flows();
        check_eq(snk->buf, std::vector<int>{});
        check(snk->completed());
      }
    }
    WHEN(
      "calling on_complete(control) on a throttle_first without pending data") {
      THEN("throttle_first raises an error after shipping pending items") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(make_observable().never<int>(),
                           make_observable().never<int64_t>(),
                           snk->as_observer());
        snk->request(42);
        run_flows();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        uut->fwd_on_next(fwd_data, 3);
        uut->fwd_on_next(fwd_ctrl, 1);
        check_eq(uut->pending(), false);
        uut->fwd_on_complete(fwd_ctrl);
        check_eq(snk->buf, std::vector<int>{1});
        check(snk->aborted());
      }
    }
    WHEN("calling on_complete(control) on a throttle_first with pending data") {
      THEN("throttle_first raises an error immediately") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(make_observable().never<int>(),
                           make_observable().never<int64_t>(),
                           snk->as_observer());
        run_flows();
        uut->fwd_on_next(fwd_data, 1);
        uut->fwd_on_next(fwd_data, 2);
        check_eq(uut->pending(), true);
        uut->fwd_on_complete(fwd_ctrl);
        check(snk->buf.empty());
        check(!snk->completed());
        snk->request(42);
        uut->dispose();
        run_flows();
        check_eq(snk->buf, std::vector<int>{});
        check(snk->aborted());
      }
    }
  }
}

SCENARIO("disposing a throttle_first operator completes the flow") {
  GIVEN("a throttle_first operator") {
    WHEN("disposing the subscription operator of the operator") {
      THEN("the observer receives an on_complete event") {
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(make_observable().never<int>(),
                           make_observable().never<int64_t>(),
                           snk->as_observer());
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
