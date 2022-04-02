// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.merge

#include "caf/flow/merge.hpp"

#include "core-test.hpp"

#include "caf/flow/observable_builder.hpp"
#include "caf/flow/scoped_coordinator.hpp"

using namespace caf;

namespace {

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();

  template <class... Ts>
  std::vector<int> ls(Ts... xs) {
    return std::vector<int>{xs...};
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("mergers round-robin over their inputs") {
  GIVEN("a merger with no inputs and shutdown-on-last-complete ON") {
    auto uut = make_counted<flow::merger_impl<int>>(ctx.get());
    WHEN("subscribing to the merger") {
      THEN("the merger immediately closes") {
        auto snk = flow::make_passive_observer<int>();
        uut->subscribe(snk->as_observer());
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::aborted);
        CHECK_EQ(snk->err, sec::disposed);
        CHECK(snk->buf.empty());
      }
    }
  }
  GIVEN("a merger with no inputs and shutdown-on-last-complete OFF") {
    auto uut = make_counted<flow::merger_impl<int>>(ctx.get());
    uut->shutdown_on_last_complete(false);
    WHEN("subscribing to the merger") {
      THEN("the merger accepts the subscription and does nothing else") {
        auto snk = flow::make_passive_observer<int>();
        uut->subscribe(snk->as_observer());
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK(snk->buf.empty());
        uut->dispose();
        ctx->run();
      }
    }
  }
  GIVEN("a round-robin merger with one input that completes") {
    WHEN("subscribing to the merger and requesting before the first push") {
      auto uut = make_counted<flow::merger_impl<int>>(ctx.get());
      auto src = flow::make_passive_observable<int>(ctx.get());
      uut->add(src->as_observable());
      ctx->run();
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
        src->push(1, 2, 3, 4, 5);
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK_EQ(snk->buf, ls(1, 2));
        MESSAGE("when requesting more data, the observer gets the remainder");
        snk->sub.request(20);
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK_EQ(snk->buf, ls(1, 2, 3, 4, 5));
        MESSAGE("the merger closes if the source closes");
        src->complete();
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::completed);
        CHECK_EQ(snk->buf, ls(1, 2, 3, 4, 5));
      }
    }
    AND_WHEN("subscribing to the merger pushing before the first request") {
      auto uut = make_counted<flow::merger_impl<int>>(ctx.get());
      auto src = flow::make_passive_observable<int>(ctx.get());
      uut->add(src->as_observable());
      ctx->run();
      auto snk = flow::make_passive_observer<int>();
      uut->subscribe(snk->as_observer());
      ctx->run();
      THEN("the merger forwards all items from the source") {
        MESSAGE("the observer enters the state subscribed");
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK_EQ(snk->buf, ls());
        MESSAGE("after pushing, the observer receives nothing yet");
        src->push(1, 2, 3, 4, 5);
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
        src->complete();
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::completed);
        CHECK_EQ(snk->buf, ls(1, 2, 3, 4, 5));
      }
    }
  }
  GIVEN("a round-robin merger with one input that aborts after some items") {
    WHEN("subscribing to the merger") {
      auto uut = make_counted<flow::merger_impl<int>>(ctx.get());
      auto src = flow::make_passive_observable<int>(ctx.get());
      uut->add(src->as_observable());
      ctx->run();
      auto snk = flow::make_passive_observer<int>();
      uut->subscribe(snk->as_observer());
      ctx->run();
      THEN("the merger forwards all items from the source until the error") {
        MESSAGE("after the source pushed five items, it emits an error");
        src->push(1, 2, 3, 4, 5);
        ctx->run();
        src->abort(make_error(sec::runtime_error));
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
}

SCENARIO("merge operators combine inputs") {
  GIVEN("two observables") {
    WHEN("merging them to a single publisher") {
      THEN("the observer receives the output of both sources") {
        auto on_complete_called = false;
        auto outputs = std::vector<int>{};
        auto r1 = ctx->make_observable().repeat(11).take(113);
        auto r2 = ctx->make_observable().repeat(22).take(223);
        flow::merge(std::move(r1), std::move(r2))
          .for_each([&outputs](int x) { outputs.emplace_back(x); },
                    [](const error& err) { FAIL("unexpected error:" << err); },
                    [&on_complete_called] { on_complete_called = true; });
        ctx->run();
        CHECK(on_complete_called);
        if (CHECK_EQ(outputs.size(), 336u)) {
          std::sort(outputs.begin(), outputs.end());
          CHECK(std::all_of(outputs.begin(), outputs.begin() + 113,
                            [](int x) { return x == 11; }));
          CHECK(std::all_of(outputs.begin() + 113, outputs.end(),
                            [](int x) { return x == 22; }));
        }
      }
    }
  }
}

SCENARIO("mergers can delay shutdown") {
  GIVEN("a merger with two inputs and shutdown_on_last_complete set to false") {
    WHEN("both inputs completed") {
      THEN("the merger only closes after enabling shutdown_on_last_complete") {
        auto on_complete_called = false;
        auto outputs = std::vector<int>{};
        auto merger = make_counted<flow::merger_impl<int>>(ctx.get());
        merger->shutdown_on_last_complete(false);
        merger->add(ctx->make_observable().repeat(11).take(113));
        merger->add(ctx->make_observable().repeat(22).take(223));
        merger //
          ->as_observable()
          .for_each([&outputs](int x) { outputs.emplace_back(x); },
                    [](const error& err) { FAIL("unexpected error:" << err); },
                    [&on_complete_called] { on_complete_called = true; });
        ctx->run();
        CHECK(!on_complete_called);
        if (CHECK_EQ(outputs.size(), 336u)) {
          std::sort(outputs.begin(), outputs.end());
          CHECK(std::all_of(outputs.begin(), outputs.begin() + 113,
                            [](int x) { return x == 11; }));
          CHECK(std::all_of(outputs.begin() + 113, outputs.end(),
                            [](int x) { return x == 22; }));
        }
        merger->shutdown_on_last_complete(true);
        ctx->run();
        CHECK(on_complete_called);
      }
    }
  }
}

END_FIXTURE_SCOPE()
