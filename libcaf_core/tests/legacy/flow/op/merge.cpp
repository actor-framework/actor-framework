// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.op.merge

#include "caf/flow/op/merge.hpp"

#include "caf/flow/multicaster.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/scoped_coordinator.hpp"

#include "core-test.hpp"

using namespace caf;

namespace {

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();

  auto make_observable() {
    return ctx->make_observable();
  }

  template <class... Ts>
  std::vector<int> ls(Ts... xs) {
    return std::vector<int>{xs...};
  }

  template <class T>
  std::vector<T> concat(std::vector<T> xs, std::vector<T> ys) {
    for (auto& y : ys)
      xs.push_back(y);
    return xs;
  }

  // Creates a flow::op::merge<T>
  template <class T, class... Inputs>
  auto make_operator(Inputs... inputs) {
    using impl_t = flow::op::merge<T>;
    return make_counted<impl_t>(ctx.get(),
                                std::move(inputs).as_observable()...);
  }

  // Similar to merge::subscribe, but returns a merge_sub pointer instead of
  // type-erasing it into a disposable.
  template <class T>
  auto raw_sub(flow::observer<T> out) {
    using flow::observable;
    auto ptr = make_counted<flow::op::merge_sub<T>>(ctx.get(), out, 8, 8);
    out.on_subscribe(flow::subscription{ptr});
    return ptr;
  }

  // Similar to merge::subscribe, but returns a merge_sub pointer instead of
  // type-erasing it into a disposable.
  template <class T, class... Inputs>
  auto raw_sub(flow::observer<T> out, Inputs... inputs) {
    using impl_t = flow::op::merge_sub<T>;
    auto merge = flow::op::merge<T>{ctx.get(),
                                    std::move(inputs).as_observable()...};
    auto res = merge.subscribe(std::move(out));
    return intrusive_ptr<impl_t>{static_cast<impl_t*>(res.ptr())};
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("the merge operator combines inputs") {
  GIVEN("two successful observables") {
    WHEN("merging them to a single observable") {
      THEN("the observer receives the output of both sources") {
        using ivec = std::vector<int>;
        auto snk = flow::make_auto_observer<int>();
        make_observable()
          .repeat(11)
          .take(113)
          .merge(make_observable().repeat(22).take(223))
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::completed);
        CHECK_EQ(snk->sorted_buf(), concat(ivec(113, 11), ivec(223, 22)));
      }
    }
  }
  GIVEN("one fail observable with one successful observable") {
    WHEN("merging them to a single observable") {
      THEN("the observer aborts with error") {
        auto snk = flow::make_auto_observer<int>();
        make_observable()
          .fail<int>(sec::runtime_error)
          .merge(make_observable().repeat(22).take(223))
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::aborted);
        CHECK_EQ(snk->err, sec::runtime_error);
      }
    }
  }
  GIVEN("two fail observables") {
    WHEN("merging them to a single observable") {
      THEN("the observer receives the error of first observable") {
        auto snk = flow::make_auto_observer<int>();
        make_observable()
          .fail<int>(sec::runtime_error)
          .merge(make_observable().fail<int>(sec::end_of_stream))
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::aborted);
        CHECK_EQ(snk->err, sec::runtime_error);
      }
    }
  }
}

SCENARIO("mergers round-robin over their inputs") {
  GIVEN("a merger with inputs observables that produce no inputs") {
    WHEN("subscribing to the merger") {
      THEN("the merger immediately closes") {
        auto nil = make_observable().empty<int>().as_observable();
        auto uut = flow::make_observable<flow::op::merge<int>>(ctx.get(), nil,
                                                               nil);
        auto snk = flow::make_auto_observer<int>();
        uut.subscribe(snk->as_observer());
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::completed);
        CHECK(snk->buf.empty());
      }
    }
  }
  GIVEN("a merger with one input that completes") {
    WHEN("subscribing to the merger and requesting before the first push") {
      auto src = flow::multicaster<int>{ctx.get()};
      auto nil = make_observable().empty<int>().as_observable();
      auto uut = make_counted<flow::op::merge<int>>(ctx.get(),
                                                    src.as_observable(), nil);
      auto snk = flow::make_passive_observer<int>();
      uut->subscribe(snk->as_observer());
      ctx->run();
      THEN("the merger forwards all items from the source") {
        MESSAGE("the observer enters the state subscribed");
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK_EQ(snk->buf, ls());
        MESSAGE("when requesting data, no data is received yet");
        snk->sub.request(2);
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK_EQ(snk->buf, ls());
        MESSAGE("after pushing, the observer immediately receives them");
        src.push({1, 2, 3, 4, 5});
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK_EQ(snk->buf, ls(1, 2));
        MESSAGE("when requesting more data, the observer gets the remainder");
        snk->sub.request(20);
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK_EQ(snk->buf, ls(1, 2, 3, 4, 5));
        MESSAGE("the merger closes if the source closes");
        src.close();
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::completed);
        CHECK_EQ(snk->buf, ls(1, 2, 3, 4, 5));
      }
    }
    WHEN("subscribing to the merger pushing before the first request") {
      auto src = flow::multicaster<int>{ctx.get()};
      auto nil = make_observable().empty<int>().as_observable();
      auto uut = make_counted<flow::op::merge<int>>(ctx.get(),
                                                    src.as_observable(), nil);
      ctx->run();
      auto snk = flow::make_passive_observer<int>();
      uut->subscribe(snk->as_observer());
      ctx->run();
      THEN("the merger forwards all items from the source") {
        MESSAGE("the observer enters the state subscribed");
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK_EQ(snk->buf, ls());
        MESSAGE("after pushing, the observer receives nothing yet");
        src.push({1, 2, 3, 4, 5});
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK_EQ(snk->buf, ls());
        MESSAGE("the observer get the first items immediately when requesting");
        snk->sub.request(2);
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK_EQ(snk->buf, ls(1, 2));
        MESSAGE("when requesting more data, the observer gets the remainder");
        snk->sub.request(20);
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK_EQ(snk->buf, ls(1, 2, 3, 4, 5));
        MESSAGE("the merger closes if the source closes");
        src.close();
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::completed);
        CHECK_EQ(snk->buf, ls(1, 2, 3, 4, 5));
      }
    }
  }
  GIVEN("a merger with one input that aborts after some items") {
    WHEN("subscribing to the merger") {
      auto src = flow::multicaster<int>{ctx.get()};
      auto nil = make_observable().empty<int>().as_observable();
      auto uut = make_counted<flow::op::merge<int>>(ctx.get(),
                                                    src.as_observable(), nil);
      auto snk = flow::make_passive_observer<int>();
      uut->subscribe(snk->as_observer());
      ctx->run();
      THEN("the merger forwards all items from the source until the error") {
        MESSAGE("after the source pushed five items, it emits an error");
        src.push({1, 2, 3, 4, 5});
        ctx->run();
        src.abort(make_error(sec::runtime_error));
        ctx->run();
        MESSAGE("when requesting, the observer still obtains the items first");
        snk->sub.request(2);
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK_EQ(snk->buf, ls(1, 2));
        snk->sub.request(20);
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::aborted);
        CHECK_EQ(snk->buf, ls(1, 2, 3, 4, 5));
        CHECK_EQ(snk->err, make_error(sec::runtime_error));
      }
    }
  }
  GIVEN("a merger that operates on an observable of observables") {
    WHEN("subscribing to the merger") {
      THEN("the subscribers receives all values from all observables") {
        auto inputs = std::vector<flow::observable<int>>{
          make_observable().iota(1).take(3).as_observable(),
          make_observable().iota(4).take(3).as_observable(),
          make_observable().iota(7).take(3).as_observable(),
        };
        auto snk = flow::make_auto_observer<int>();
        make_observable()
          .from_container(std::move(inputs))
          .merge()
          .subscribe(snk->as_observer());
        ctx->run();
        std::sort(snk->buf.begin(), snk->buf.end());
        CHECK_EQ(snk->buf, ls(1, 2, 3, 4, 5, 6, 7, 8, 9));
      }
    }
  }
}

SCENARIO("empty merge operators only call on_complete") {
  GIVEN("a merge operator with no inputs") {
    WHEN("subscribing to it") {
      THEN("the observer only receives an on_complete event") {
        auto nil = make_observable() //
                     .empty<flow::observable<int>>()
                     .as_observable();
        auto snk = flow::make_auto_observer<int>();
        auto sub = make_operator<int>(nil)->subscribe(snk->as_observer());
        ctx->run();
        CHECK(sub.disposed());
        CHECK(snk->completed());
        CHECK(snk->buf.empty());
      }
    }
  }
}

SCENARIO("the merge operator disposes unexpected subscriptions") {
  GIVEN("a merge operator with no inputs") {
    WHEN("subscribing to it") {
      THEN("the observer only receives an on_complete event") {
        auto snk = flow::make_passive_observer<int>();
        auto r1 = make_observable().just(1).as_observable();
        auto r2 = make_observable().just(2).as_observable();
        auto uut = raw_sub(snk->as_observer(), r1, r2);
        auto sub = make_counted<flow::passive_subscription_impl>();
        ctx->run();
        CHECK(!sub->disposed());
        uut->fwd_on_subscribe(42, flow::subscription{sub});
        CHECK(sub->disposed());
        snk->request(127);
        ctx->run();
        CHECK(snk->completed());
        CHECK_EQ(snk->buf, std::vector<int>({1, 2}));
      }
    }
  }
}

SCENARIO("the merge operator emits already buffered data on error") {
  GIVEN("an observable source that emits an error after the first observable") {
    WHEN("the error occurs while data is buffered") {
      THEN("the merger forwards the buffered items before the error") {
        auto src = flow::multicaster<flow::observable<int>>{ctx.get()};
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(snk->as_observer(), src.as_observable());
        // First observable emits 3 items and then does nothing.
        src.push(make_observable()
                   .iota(1) //
                   .take(3)
                   .concat(make_observable().never<int>()));
        ctx->run();
        CHECK_EQ(uut->buffered(), 3u);
        CHECK_EQ(uut->num_inputs(), 1u);
        // Emit an error to the merge operator.
        src.abort(make_error(sec::runtime_error));
        ctx->run();
        CHECK_EQ(uut->buffered(), 3u);
        CHECK_EQ(snk->buf, ls());
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        // Pull buffered items from the merge operator.
        snk->sub.request(5);
        ctx->run();
        CHECK_EQ(uut->num_inputs(), 0u);
        CHECK_EQ(snk->buf, ls(1, 2, 3));
        CHECK_EQ(snk->state, flow::observer_state::aborted);
      }
    }
    WHEN("the error occurs while no data is buffered") {
      THEN("the merger forwards the error immediately") {
        auto src = flow::multicaster<flow::observable<int>>{ctx.get()};
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(snk->as_observer(), src.as_observable());
        // First observable emits 3 items and then does nothing.
        src.push(make_observable()
                   .iota(1) //
                   .take(3)
                   .concat(make_observable().never<int>()));
        ctx->run();
        CHECK_EQ(uut->buffered(), 3u);
        CHECK_EQ(uut->num_inputs(), 1u);
        // Pull buffered items from the merge operator.
        snk->sub.request(5);
        ctx->run();
        CHECK_EQ(snk->buf, ls(1, 2, 3));
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        // Emit an error to the merge operator.
        src.abort(make_error(sec::runtime_error));
        CHECK_EQ(snk->state, flow::observer_state::aborted);
      }
    }
  }
  GIVEN("an input observable that emits an error after emitting some items") {
    WHEN("the error occurs while data is buffered") {
      THEN("the merger forwards the buffered items before the error") {
        auto src = flow::multicaster<int>{ctx.get()};
        auto nil = make_observable().never<int>().as_observable();
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(snk->as_observer(), src.as_observable(), nil);
        ctx->run();
        src.push({1, 2, 3, 4, 5, 6, 7});
        ctx->run();
        CHECK_EQ(uut->buffered(), 7u);
        src.abort(make_error(sec::runtime_error));
        ctx->run();
        CHECK_EQ(uut->buffered(), 7u);
        snk->sub.request(5);
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK_EQ(snk->buf, ls(1, 2, 3, 4, 5));
        CHECK(!uut->disposed());
        snk->sub.request(5);
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::aborted);
        CHECK_EQ(snk->buf, ls(1, 2, 3, 4, 5, 6, 7));
        CHECK(uut->disposed());
      }
    }
    WHEN("the error occurs while no data is buffered") {
      THEN("the merger forwards the error immediately") {
        auto src = flow::multicaster<int>{ctx.get()};
        auto nil = make_observable().never<int>().as_observable();
        auto snk = flow::make_passive_observer<int>();
        auto uut = raw_sub(snk->as_observer(), src.as_observable(), nil);
        ctx->run();
        CHECK_EQ(src.demand(), 8u);
        CHECK_EQ(src.buffered(), 0u);
        snk->sub.request(10);
        ctx->run();
        CHECK_EQ(uut->demand(), 10u);
        CHECK_EQ(src.demand(), 8u);
        CHECK_EQ(src.buffered(), 0u);
        // Push 7 items.
        CHECK_EQ(src.push({1, 2, 3, 4, 5, 6, 7}), 7u);
        CHECK_EQ(src.buffered(), 0u);
        CHECK_EQ(uut->buffered(), 0u);
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK_EQ(snk->err, sec::none);
        CHECK_EQ(snk->buf, ls(1, 2, 3, 4, 5, 6, 7));
        // Push an error.
        src.abort(make_error(sec::runtime_error));
        CHECK_EQ(snk->state, flow::observer_state::aborted);
        CHECK_EQ(snk->buf, ls(1, 2, 3, 4, 5, 6, 7));
        CHECK_EQ(snk->err, sec::runtime_error);
        CHECK(uut->disposed());
      }
    }
  }
}

SCENARIO("the merge operator drops inputs with no pending data on error") {
  GIVEN("a merge operator with two inputs") {
    WHEN("one of the inputs fails") {
      THEN("the operator drops the other input right away") {
        auto snk = flow::make_auto_observer<int>();
        auto uut = raw_sub(snk->as_observer(), make_observable().never<int>(),
                           make_observable().fail<int>(sec::runtime_error));
        ctx->run();
        CHECK(uut->disposed());
      }
    }
  }
}

SCENARIO("the merge operator drops inputs when disposed") {
  GIVEN("a merge operator with two inputs") {
    WHEN("one of the inputs fails") {
      THEN("the operator drops the other input right away") {
        auto snk = flow::make_auto_observer<int>();
        auto uut = raw_sub(snk->as_observer(), make_observable().never<int>(),
                           make_observable().never<int>());
        ctx->run();
        CHECK(!uut->disposed());
        uut->dispose();
        ctx->run();
        CHECK(uut->disposed());
      }
    }
  }
}

TEST_CASE("merge operators ignore on_subscribe calls past the first one") {
  auto snk = flow::make_auto_observer<int>();
  auto uut = raw_sub(snk->as_observer());
  CHECK(!uut->subscribed());
  make_observable()
    .just(make_observable().iota(1).take(5).as_observable())
    .subscribe(uut->as_observer());
  CHECK(uut->subscribed());
  make_observable()
    .just(make_observable().iota(10).take(5).as_observable())
    .subscribe(uut->as_observer());
  CHECK(uut->subscribed());
  ctx->run();
  CHECK_EQ(snk->buf, ls(1, 2, 3, 4, 5));
}

TEST_CASE("merge operators ignore fwd_on_complete calls with unknown keys") {
  auto snk = flow::make_auto_observer<int>();
  auto uut = raw_sub(snk->as_observer());
  CHECK(!uut->subscribed());
  make_observable()
    .just(make_observable().iota(1).take(5).as_observable())
    .subscribe(uut->as_observer());
  CHECK(uut->subscribed());
  uut->fwd_on_complete(42);
  CHECK(uut->subscribed());
  ctx->run();
  CHECK_EQ(snk->buf, ls(1, 2, 3, 4, 5));
}

TEST_CASE("merge operators ignore fwd_on_error calls with unknown keys") {
  auto snk = flow::make_auto_observer<int>();
  auto uut = raw_sub(snk->as_observer());
  CHECK(!uut->subscribed());
  make_observable()
    .just(make_observable().iota(1).take(5).as_observable())
    .subscribe(uut->as_observer());
  CHECK(uut->subscribed());
  uut->fwd_on_error(42, make_error(sec::runtime_error));
  CHECK(uut->subscribed());
  ctx->run();
  CHECK_EQ(snk->buf, ls(1, 2, 3, 4, 5));
  CHECK_EQ(snk->state, flow::observer_state::completed);
}

TEST_CASE("the merge operator merges any number of input observables") {
  auto snk = flow::make_passive_observer<int>();
  auto inputs = std::vector<flow::observable<int>>{};
  for (int i = 0; i < 1'000; ++i)
    inputs.push_back(make_observable().just(i).as_observable());
  auto uut = raw_sub(snk->as_observer(),
                     make_observable().from_container(std::move(inputs)));
  ctx->run();
  CHECK_EQ(uut->max_concurrent(), 8u);
  CHECK_EQ(uut->num_inputs(), 8u);
  snk->sub.request(10);
  ctx->run();
  CHECK_EQ(uut->max_concurrent(), 8u);
  CHECK_EQ(uut->num_inputs(), 8u);
  CHECK_EQ(snk->buf.size(), 10u);
  CHECK_EQ(snk->sorted_buf(), ls(0, 1, 2, 3, 4, 5, 6, 7, 8, 9));
  snk->sub.request(10'000);
  ctx->run();
  CHECK_EQ(snk->buf.size(), 1'000u);
  CHECK_EQ(snk->state, flow::observer_state::completed);
}

TEST_CASE("the merge operator ignores request() calls with no subscriber") {
  auto snk = flow::make_auto_observer<int>();
  auto uut = raw_sub(snk->as_observer());
  make_observable()
    .just(make_observable().iota(1).take(5).as_observable())
    .subscribe(uut->as_observer());
  ctx->run();
  auto pre = uut->demand();
  uut->request(10);
  CHECK_EQ(uut->demand(), pre);
  CHECK_EQ(snk->buf, ls(1, 2, 3, 4, 5));
}

END_FIXTURE_SCOPE()
