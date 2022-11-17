// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.merge

#include "caf/flow/merge.hpp"

#include "core-test.hpp"

#include "caf/flow/item_publisher.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/op/merge.hpp"
#include "caf/flow/scoped_coordinator.hpp"

using namespace caf;

namespace {

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();

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
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("mergers round-robin over their inputs") {
  GIVEN("a merger with no inputs") {
    auto uut = flow::make_observable<flow::op::merge<int>>(ctx.get());
    WHEN("subscribing to the merger") {
      THEN("the merger immediately closes") {
        auto snk = flow::make_auto_observer<int>();
        uut.subscribe(snk->as_observer());
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::completed);
        CHECK(snk->buf.empty());
      }
    }
  }
  GIVEN("a round-robin merger with one input that completes") {
    WHEN("subscribing to the merger and requesting before the first push") {
      auto src = flow::item_publisher<int>{ctx.get()};
      auto uut = make_counted<flow::op::merge<int>>(ctx.get(),
                                                    src.as_observable());
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
    AND_WHEN("subscribing to the merger pushing before the first request") {
      auto src = flow::item_publisher<int>{ctx.get()};
      auto uut = make_counted<flow::op::merge<int>>(ctx.get(),
                                                    src.as_observable());
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
  GIVEN("a round-robin merger with one input that aborts after some items") {
    WHEN("subscribing to the merger") {
      auto src = flow::item_publisher<int>{ctx.get()};
      auto uut = make_counted<flow::op::merge<int>>(ctx.get(),
                                                    src.as_observable());
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
          ctx->make_observable().iota(1).take(3).as_observable(),
          ctx->make_observable().iota(4).take(3).as_observable(),
          ctx->make_observable().iota(7).take(3).as_observable(),
        };
        auto snk = flow::make_auto_observer<int>();
        ctx->make_observable()
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

SCENARIO("the merge operator combine inputs") {
  GIVEN("two observables") {
    WHEN("merging them to a single observable") {
      THEN("the observer receives the output of both sources") {
        using ivec = std::vector<int>;
        auto snk = flow::make_auto_observer<int>();
        ctx->make_observable()
          .repeat(11)
          .take(113)
          .merge(ctx->make_observable().repeat(22).take(223))
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::completed);
        CHECK_EQ(snk->sorted_buf(), concat(ivec(113, 11), ivec(223, 22)));
      }
    }
  }
}

END_FIXTURE_SCOPE()
