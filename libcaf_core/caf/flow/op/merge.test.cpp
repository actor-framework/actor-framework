// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/op/merge.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/flow/multicaster.hpp"
#include "caf/log/test.hpp"

using namespace caf;

namespace {

struct fixture : test::fixture::deterministic, test::fixture::flow {
  template <class T>
  std::vector<T> concat(std::vector<T> xs, std::vector<T> ys) {
    for (auto& y : ys)
      xs.push_back(y);
    return xs;
  }

  // Creates a flow::op::merge<T>
  template <class T, class... Inputs>
  auto make_operator(Inputs... inputs) {
    using impl_t = caf::flow::op::merge<T>;
    return make_counted<impl_t>(coordinator(),
                                std::move(inputs).as_observable()...);
  }

  // Similar to merge::subscribe, but returns a merge_sub pointer instead of
  // type-erasing it into a disposable.
  template <class T>
  auto raw_sub(caf::flow::observer<T> out) {
    auto ptr = make_counted<caf::flow::op::merge_sub<T>>(coordinator(), out, 8,
                                                         8);
    out.on_subscribe(caf::flow::subscription{ptr});
    return ptr;
  }

  // Similar to merge::subscribe, but returns a merge_sub pointer instead of
  // type-erasing it into a disposable.
  template <class T, class... Inputs>
  auto raw_sub(caf::flow::observer<T> out, Inputs... inputs) {
    using impl_t = caf::flow::op::merge_sub<T>;
    auto merge = caf::flow::op::merge<T>{coordinator(),
                                         std::move(inputs).as_observable()...};
    auto res = merge.subscribe(std::move(out));
    return intrusive_ptr<impl_t>{static_cast<impl_t*>(res.ptr())};
  }

  template <class T>
  auto make_never_sub(caf::flow::observer<T> out) {
    return make_counted<caf::flow::op::never_sub<T>>(coordinator(), out);
  }
};

WITH_FIXTURE(fixture) {

SCENARIO("the merge operator combines inputs") {
  GIVEN("two successful observables") {
    WHEN("merging them to a single observable") {
      THEN("the observer receives the output of both sources") {
        using ivec = std::vector<int>;
        using snk_t = flow::auto_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        make_observable()
          .repeat(11)
          .take(113)
          .merge(make_observable().repeat(22).take(223))
          .subscribe(snk->as_observer());
        run_flows();
        check_eq(snk->state, flow::observer_state::completed);
        check_eq(snk->sorted_buf(), concat(ivec(113, 11), ivec(223, 22)));
      }
    }
  }
  GIVEN("one fail observable with one successful observable") {
    WHEN("merging them to a single observable") {
      THEN("the observer aborts with error") {
        using snk_t = flow::auto_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        make_observable()
          .fail<int>(sec::runtime_error)
          .merge(make_observable().repeat(22).take(223))
          .subscribe(snk->as_observer());
        run_flows();
        check_eq(snk->state, flow::observer_state::aborted);
      }
    }
  }
  GIVEN("two fail observables") {
    WHEN("merging them to a single observable") {
      THEN("the observer receives the error of first observable") {
        using snk_t = flow::auto_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        make_observable()
          .fail<int>(sec::runtime_error)
          .merge(make_observable().fail<int>(sec::end_of_stream))
          .subscribe(snk->as_observer());
        run_flows();
        check_eq(snk->state, flow::observer_state::aborted);
      }
    }
  }
}

SCENARIO("mergers round-robin over their inputs") {
  GIVEN("a merger with inputs observables that produce no inputs") {
    WHEN("subscribing to the merger") {
      THEN("the merger immediately closes") {
        using ops_t = caf::flow::op::merge<int>;
        using snk_t = auto_observer<int>;
        auto nil = make_observable().empty<int>().as_observable();
        auto uut = coordinator()->add_child_hdl(std::in_place_type<ops_t>, nil,
                                                nil);
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        uut.subscribe(snk->as_observer());
        run_flows();
        check_eq(snk->state, flow::observer_state::completed);
        check(snk->buf.empty());
      }
    }
  }
  GIVEN("a merger with one input that completes") {
    WHEN("subscribing to the merger and requesting before the first push") {
      using snk_t = passive_observer<int>;
      auto src = caf::flow::multicaster<int>{coordinator()};
      auto nil = make_observable().empty<int>().as_observable();
      auto uut = make_counted<caf::flow::op::merge<int>>(coordinator(),
                                                         src.as_observable(),
                                                         nil);
      auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
      uut->subscribe(snk->as_observer());
      run_flows();
      THEN("the merger forwards all items from the source") {
        log::test::debug("the observer enters the state subscribed");
        check_eq(snk->state, flow::observer_state::subscribed);
        check_eq(snk->buf, std::vector<int>{});
        log::test::debug("when requesting data, no data is received yet");
        snk->sub.request(2);
        run_flows();
        check_eq(snk->state, flow::observer_state::subscribed);
        check_eq(snk->buf, std::vector<int>{});
        log::test::debug(
          "after pushing, the observer immediately receives them");
        src.push({1, 2, 3, 4, 5});
        run_flows();
        check_eq(snk->state, flow::observer_state::subscribed);
        check_eq(snk->buf, std::vector{1, 2});
        log::test::debug(
          "when requesting more data, the observer gets the remainder");
        snk->sub.request(20);
        run_flows();
        check_eq(snk->state, flow::observer_state::subscribed);
        check_eq(snk->buf, std::vector{1, 2, 3, 4, 5});
        log::test::debug("the merger closes if the source closes");
        src.close();
        run_flows();
        check_eq(snk->state, flow::observer_state::completed);
        check_eq(snk->buf, std::vector{1, 2, 3, 4, 5});
      }
    }
    WHEN("subscribing to the merger pushing before the first request") {
      using ops_t = caf::flow::op::merge<int>;
      using snk_t = passive_observer<int>;
      auto src = caf::flow::multicaster<int>{coordinator()};
      auto nil = make_observable().empty<int>().as_observable();
      auto uut = coordinator()->add_child(std::in_place_type<ops_t>,
                                          src.as_observable(), nil);
      run_flows();
      auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
      uut->subscribe(snk->as_observer());
      run_flows();
      THEN("the merger forwards all items from the source") {
        log::test::debug("the observer enters the state subscribed");
        check_eq(snk->state, flow::observer_state::subscribed);
        check_eq(snk->buf, std::vector<int>{});
        log::test::debug("after pushing, the observer receives nothing yet");
        src.push({1, 2, 3, 4, 5});
        run_flows();
        check_eq(snk->state, flow::observer_state::subscribed);
        check_eq(snk->buf, std::vector<int>{});
        log::test::debug(
          "the observer get the first items immediately when requesting");
        snk->sub.request(2);
        run_flows();
        check_eq(snk->state, flow::observer_state::subscribed);
        check_eq(snk->buf, std::vector{1, 2});
        log::test::debug(
          "when requesting more data, the observer gets the remainder");
        snk->sub.request(20);
        run_flows();
        check_eq(snk->state, flow::observer_state::subscribed);
        check_eq(snk->buf, std::vector{1, 2, 3, 4, 5});
        log::test::debug("the merger closes if the source closes");
        src.close();
        run_flows();
        check_eq(snk->state, flow::observer_state::completed);
        check_eq(snk->buf, std::vector{1, 2, 3, 4, 5});
      }
    }
  }
  GIVEN("a merger with one input that aborts after some items") {
    WHEN("subscribing to the merger") {
      using ops_t = caf::flow::op::merge<int>;
      using snk_t = passive_observer<int>;
      auto src = caf::flow::multicaster<int>{coordinator()};
      auto nil = make_observable().empty<int>().as_observable();
      auto uut = coordinator()->add_child(std::in_place_type<ops_t>,
                                          src.as_observable(), nil);
      auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
      uut->subscribe(snk->as_observer());
      run_flows();
      THEN("the merger forwards all items from the source until the error") {
        log::test::debug(
          "after the source pushed five items, it emits an error");
        src.push({1, 2, 3, 4, 5});
        run_flows();
        src.abort(error{sec::runtime_error});
        run_flows();
        log::test::debug(
          "when requesting, the observer still obtains the items first");
        snk->sub.request(2);
        run_flows();
        check_eq(snk->state, flow::observer_state::subscribed);
        check_eq(snk->buf, std::vector{1, 2});
        snk->sub.request(20);
        run_flows();
        check_eq(snk->state, flow::observer_state::aborted);
        check_eq(snk->buf, std::vector{1, 2, 3, 4, 5});
        check_eq(snk->err, error{sec::runtime_error});
      }
    }
  }
  GIVEN("a merger that operates on an observable of observables") {
    WHEN("subscribing to the merger") {
      THEN("the subscribers receives all values from all observables") {
        using snk_t = auto_observer<int>;
        auto inputs = std::vector<caf::flow::observable<int>>{
          make_observable().iota(1).take(3).as_observable(),
          make_observable().iota(4).take(3).as_observable(),
          make_observable().iota(7).take(3).as_observable(),
        };
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        make_observable()
          .from_container(std::move(inputs))
          .merge()
          .subscribe(snk->as_observer());
        run_flows();
        std::sort(snk->buf.begin(), snk->buf.end());
        check_eq(snk->buf, std::vector{1, 2, 3, 4, 5, 6, 7, 8, 9});
      }
    }
  }
}

SCENARIO("empty merge operators only call on_complete") {
  GIVEN("a merge operator with no inputs") {
    WHEN("subscribing to it") {
      THEN("the observer only receives an on_complete event") {
        using snk_t = flow::auto_observer<int>;
        auto nil = make_observable() //
                     .empty<caf::flow::observable<int>>()
                     .as_observable();
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto sub = make_operator<int>(nil)->subscribe(snk->as_observer());
        run_flows();
        check(sub.disposed());
        check(snk->completed());
        check(snk->buf.empty());
      }
    }
  }
}

SCENARIO("the merge operator disposes unexpected subscriptions") {
  GIVEN("a merge operator with no inputs") {
    WHEN("subscribing to it") {
      THEN("the observer only receives an on_complete event") {
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto r1 = make_observable().just(1).as_observable();
        auto r2 = make_observable().just(2).as_observable();
        auto uut = raw_sub(snk->as_observer(), r1, r2);
        using sub_t = caf::flow::op::never_sub<int>;
        auto sub = coordinator()->add_child(std::in_place_type<sub_t>,
                                            snk->as_observer());
        run_flows();
        check(!sub->disposed());
        uut->fwd_on_subscribe(42, caf::flow::subscription{sub});
        check(sub->disposed());
        snk->request(127);
        run_flows();
        check(snk->completed());
        check_eq(snk->buf, std::vector<int>({1, 2}));
      }
    }
  }
}

SCENARIO("the merge operator emits already buffered data on error") {
  using snk_t = flow::passive_observer<int>;
  GIVEN("an observable source that emits an error after the first observable") {
    WHEN("the error occurs while data is buffered") {
      THEN("the merger forwards the buffered items before the error") {
        auto src
          = caf::flow::multicaster<caf::flow::observable<int>>{coordinator()};
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = raw_sub(snk->as_observer(), src.as_observable());
        // First observable emits 3 items and then does nothing.
        src.push(make_observable()
                   .iota(1) //
                   .take(3)
                   .concat(make_observable().never<int>()));
        run_flows();
        check_eq(uut->buffered(), 3u);
        check_eq(uut->num_inputs(), 1u);
        // Emit an error to the merge operator.
        src.abort(error{sec::runtime_error});
        run_flows();
        check_eq(uut->buffered(), 3u);
        check_eq(snk->buf, std::vector<int>{});
        check_eq(snk->state, flow::observer_state::subscribed);
        // Pull buffered items from the merge operator.
        snk->sub.request(5);
        run_flows();
        check_eq(uut->num_inputs(), 0u);
        check_eq(snk->buf, std::vector{1, 2, 3});
        check_eq(snk->state, flow::observer_state::aborted);
      }
    }
    WHEN("the error occurs while no data is buffered") {
      THEN("the merger forwards the error immediately") {
        auto src
          = caf::flow::multicaster<caf::flow::observable<int>>{coordinator()};
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = raw_sub(snk->as_observer(), src.as_observable());
        // First observable emits 3 items and then does nothing.
        src.push(make_observable()
                   .iota(1) //
                   .take(3)
                   .concat(make_observable().never<int>()));
        run_flows();
        check_eq(uut->buffered(), 3u);
        check_eq(uut->num_inputs(), 1u);
        // Pull buffered items from the merge operator.
        snk->sub.request(5);
        run_flows();
        check_eq(snk->buf, std::vector{1, 2, 3});
        check_eq(snk->state, flow::observer_state::subscribed);
        // Emit an error to the merge operator.
        src.abort(error{sec::runtime_error});
        check_eq(snk->state, flow::observer_state::aborted);
      }
    }
  }
  GIVEN("an input observable that emits an error after emitting some items") {
    WHEN("the error occurs while data is buffered") {
      THEN("the merger forwards the buffered items before the error") {
        auto src = caf::flow::multicaster<int>{coordinator()};
        auto nil = make_observable().never<int>().as_observable();
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = raw_sub(snk->as_observer(), src.as_observable(), nil);
        run_flows();
        src.push({1, 2, 3, 4, 5, 6, 7});
        run_flows();
        check_eq(uut->buffered(), 7u);
        src.abort(error{sec::runtime_error});
        run_flows();
        check_eq(uut->buffered(), 7u);
        snk->sub.request(5);
        run_flows();
        check_eq(snk->state, flow::observer_state::subscribed);
        check_eq(snk->buf, std::vector{1, 2, 3, 4, 5});
        check(!uut->disposed());
        snk->sub.request(5);
        run_flows();
        check_eq(snk->state, flow::observer_state::aborted);
        check_eq(snk->buf, std::vector{1, 2, 3, 4, 5, 6, 7});
        check(uut->disposed());
      }
    }
    WHEN("the error occurs while no data is buffered") {
      THEN("the merger forwards the error immediately") {
        auto src = caf::flow::multicaster<int>{coordinator()};
        auto nil = make_observable().never<int>().as_observable();
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = raw_sub(snk->as_observer(), src.as_observable(), nil);
        run_flows();
        check_eq(src.demand(), 8u);
        check_eq(src.buffered(), 0u);
        snk->sub.request(10);
        run_flows();
        check_eq(uut->demand(), 10u);
        check_eq(src.demand(), 8u);
        check_eq(src.buffered(), 0u);
        // Push 7 items.
        check_eq(src.push({1, 2, 3, 4, 5, 6, 7}), 7u);
        check_eq(src.buffered(), 0u);
        check_eq(uut->buffered(), 0u);
        check_eq(snk->state, flow::observer_state::subscribed);
        check_eq(snk->err, sec::none);
        check_eq(snk->buf, std::vector{1, 2, 3, 4, 5, 6, 7});
        // Push an error.
        src.abort(error{sec::runtime_error});
        check_eq(snk->state, flow::observer_state::aborted);
        check_eq(snk->buf, std::vector{1, 2, 3, 4, 5, 6, 7});
        check_eq(snk->err, sec::runtime_error);
        check(uut->disposed());
      }
    }
  }
}

SCENARIO("the merge operator drops inputs with no pending data on error") {
  GIVEN("a merge operator with two inputs") {
    WHEN("one of the inputs fails") {
      THEN("the operator drops the other input right away") {
        using snk_t = flow::auto_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = raw_sub(snk->as_observer(), make_observable().never<int>(),
                           make_observable().fail<int>(sec::runtime_error));
        run_flows();
        check(uut->disposed());
      }
    }
  }
}

SCENARIO("the merge operator drops inputs when disposed") {
  GIVEN("a merge operator with two inputs") {
    WHEN("one of the inputs fails") {
      THEN("the operator drops the other input right away") {
        using snk_t = flow::auto_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = raw_sub(snk->as_observer(), make_observable().never<int>(),
                           make_observable().never<int>());
        run_flows();
        check(!uut->disposed());
        uut->dispose();
        run_flows();
        check(uut->disposed());
      }
    }
  }
}

TEST("merge operators ignore on_subscribe calls past the first one") {
  using snk_t = flow::auto_observer<int>;
  auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
  auto uut = raw_sub(snk->as_observer());
  check(!uut->subscribed());
  make_observable()
    .just(make_observable().iota(1).take(5).as_observable())
    .subscribe(uut->as_observer());
  check(uut->subscribed());
  make_observable()
    .just(make_observable().iota(10).take(5).as_observable())
    .subscribe(uut->as_observer());
  check(uut->subscribed());
  run_flows();
  check_eq(snk->buf, std::vector{1, 2, 3, 4, 5});
}

TEST("merge operators ignore fwd_on_complete calls with unknown keys") {
  using snk_t = flow::auto_observer<int>;
  auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
  auto uut = raw_sub(snk->as_observer());
  check(!uut->subscribed());
  make_observable()
    .just(make_observable().iota(1).take(5).as_observable())
    .subscribe(uut->as_observer());
  check(uut->subscribed());
  uut->fwd_on_complete(42);
  check(uut->subscribed());
  run_flows();
  check_eq(snk->buf, std::vector{1, 2, 3, 4, 5});
}

TEST("merge operators ignore fwd_on_error calls with unknown keys") {
  using snk_t = flow::auto_observer<int>;
  auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
  auto uut = raw_sub(snk->as_observer());
  check(!uut->subscribed());
  make_observable()
    .just(make_observable().iota(1).take(5).as_observable())
    .subscribe(uut->as_observer());
  check(uut->subscribed());
  uut->fwd_on_error(42, error{sec::runtime_error});
  check(uut->subscribed());
  run_flows();
  check_eq(snk->buf, std::vector{1, 2, 3, 4, 5});
  check_eq(snk->state, flow::observer_state::completed);
}

TEST("the merge operator merges any number of input observables") {
  using snk_t = flow::passive_observer<int>;
  auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
  auto inputs = std::vector<caf::flow::observable<int>>{};
  for (int i = 0; i < 1'000; ++i)
    inputs.push_back(make_observable().just(i).as_observable());
  auto uut = raw_sub(snk->as_observer(),
                     make_observable().from_container(std::move(inputs)));
  run_flows();
  check_eq(uut->max_concurrent(), 8u);
  check_eq(uut->num_inputs(), 8u);
  snk->sub.request(10);
  run_flows();
  check_eq(uut->max_concurrent(), 8u);
  check_eq(uut->num_inputs(), 8u);
  check_eq(snk->buf.size(), 10u);
  check_eq(snk->sorted_buf(), std::vector{0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
  snk->sub.request(10'000);
  run_flows();
  check_eq(snk->buf.size(), 1'000u);
  check_eq(snk->state, flow::observer_state::completed);
}

TEST("the merge operator ignores request() calls with no subscriber") {
  using snk_t = flow::auto_observer<int>;
  auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
  auto uut = raw_sub(snk->as_observer());
  make_observable()
    .just(make_observable().iota(1).take(5).as_observable())
    .subscribe(uut->as_observer());
  run_flows();
  auto pre = uut->demand();
  uut->request(10);
  check_eq(uut->demand(), pre);
  check_eq(snk->buf, std::vector{1, 2, 3, 4, 5});
}

} // WITH_FIXTURE(fixture)

} // namespace
