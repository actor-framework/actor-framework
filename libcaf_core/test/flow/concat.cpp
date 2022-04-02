// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.concat

#include "caf/flow/concat.hpp"

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

SCENARIO("concatenate processes inputs sequentially") {
  GIVEN("a concatenation with no inputs and shutdown-on-last-complete ON") {
    auto uut = make_counted<flow::concat_impl<int>>(ctx.get());
    WHEN("subscribing to the concatenation") {
      THEN("the concatenation immediately closes") {
        auto snk = flow::make_passive_observer<int>();
        uut->subscribe(snk->as_observer());
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::aborted);
        CHECK_EQ(snk->err, sec::disposed);
        CHECK(snk->buf.empty());
      }
    }
  }
  GIVEN("a concatenation with no inputs and shutdown-on-last-complete OFF") {
    auto uut = make_counted<flow::concat_impl<int>>(ctx.get());
    uut->shutdown_on_last_complete(false);
    WHEN("subscribing to the concatenation") {
      THEN("the concatenation accepts the subscription and does nothing else") {
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
  GIVEN("a concatenation with one input that completes") {
    WHEN("subscribing and requesting before the first push") {
      auto uut = make_counted<flow::concat_impl<int>>(ctx.get());
      auto src = flow::make_passive_observable<int>(ctx.get());
      uut->add(src->as_observable());
      ctx->run();
      auto snk = flow::make_passive_observer<int>();
      uut->subscribe(snk->as_observer());
      ctx->run();
      THEN("the concatenation forwards all items from the source") {
        MESSAGE("the observer enters the state subscribed");
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK_EQ(snk->buf, ls());
        MESSAGE("when requesting data, no data is received yet");
        snk->sub.request(2);
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK_EQ(snk->buf, ls());
        MESSAGE("after pushing, the observer immediately receives them");
        src->push(1, 2);
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK_EQ(snk->buf, ls(1, 2));
        MESSAGE("when requesting more data, the observer gets the remainder");
        snk->sub.request(20);
        ctx->run();
        src->push(3, 4, 5);
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK_EQ(snk->buf, ls(1, 2, 3, 4, 5));
        MESSAGE("the concatenation closes if the source closes");
        src->complete();
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::completed);
        CHECK_EQ(snk->buf, ls(1, 2, 3, 4, 5));
      }
    }
  }
  GIVEN("a concatenation with one input that aborts after some items") {
    WHEN("subscribing to the concatenation") {
      auto uut = make_counted<flow::concat_impl<int>>(ctx.get());
      auto src = flow::make_passive_observable<int>(ctx.get());
      uut->add(src->as_observable());
      ctx->run();
      auto snk = flow::make_passive_observer<int>();
      uut->subscribe(snk->as_observer());
      ctx->run();
      THEN("the concatenation forwards all items until the error") {
        MESSAGE("after the source pushed five items, it emits an error");
        snk->sub.request(20);
        ctx->run();
        src->push(1, 2, 3, 4, 5);
        ctx->run();
        src->abort(make_error(sec::runtime_error));
        ctx->run();
        MESSAGE("the observer obtains the and then the error");
        CHECK_EQ(snk->state, flow::observer_state::aborted);
        CHECK_EQ(snk->buf, ls(1, 2, 3, 4, 5));
        CHECK_EQ(snk->err, make_error(sec::runtime_error));
      }
    }
  }
}

SCENARIO("concat operators combine inputs") {
  GIVEN("two observables") {
    WHEN("merging them to a single publisher via concat") {
      THEN("the observer receives the output of both sources in order") {
        auto outputs = std::vector<int>{};
        auto r1 = ctx->make_observable().repeat(11).take(113);
        auto r2 = ctx->make_observable().repeat(22).take(223);
        flow::concat(std::move(r1), std::move(r2)).for_each([&outputs](int x) {
          outputs.emplace_back(x);
        });
        ctx->run();
        if (CHECK_EQ(outputs.size(), 336u)) {
          CHECK(std::all_of(outputs.begin(), outputs.begin() + 113,
                            [](int x) { return x == 11; }));
          CHECK(std::all_of(outputs.begin() + 113, outputs.end(),
                            [](int x) { return x == 22; }));
        }
      }
    }
  }
}

END_FIXTURE_SCOPE()
