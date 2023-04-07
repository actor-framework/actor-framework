// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.op.zip_with

#include "caf/flow/op/zip_with.hpp"

#include "core-test.hpp"

#include "caf/flow/observable_builder.hpp"
#include "caf/flow/scoped_coordinator.hpp"

using namespace caf;

namespace {

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();

  template <class F, class Out, class... Ts>
  auto make_zip_with_sub(F fn, flow::observer<Out> out, Ts... inputs) {
    using impl_t = flow::op::zip_with_sub<F, typename Ts::output_type...>;
    auto pack = std::make_tuple(std::move(inputs).as_observable()...);
    auto sub = make_counted<impl_t>(ctx.get(), std::move(fn), out, pack);
    out.on_subscribe(flow::subscription{sub});
    return sub;
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("zip_with combines inputs") {
  GIVEN("two observables") {
    WHEN("merging them with zip_with") {
      THEN("the observer receives the combined output of both sources") {
        auto snk = flow::make_passive_observer<int>();
        auto grd = make_unsubscribe_guard(snk);
        ctx->make_observable()
          .zip_with([](int x, int y) { return x + y; },
                    ctx->make_observable().repeat(11).take(113),
                    ctx->make_observable().repeat(22).take(223))
          .subscribe(snk->as_observer());
        ctx->run();
        REQUIRE_EQ(snk->state, flow::observer_state::subscribed);
        snk->sub.request(64);
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::subscribed);
        CHECK_EQ(snk->buf.size(), 64u);
        snk->sub.request(64);
        ctx->run();
        CHECK_EQ(snk->state, flow::observer_state::completed);
        CHECK_EQ(snk->buf.size(), 113u);
        CHECK_EQ(snk->buf, std::vector<int>(113, 33));
      }
    }
  }
}

SCENARIO("zip_with emits nothing when zipping an empty observable") {
  GIVEN("two observables, one of them empty") {
    WHEN("merging them with zip_with") {
      THEN("the observer sees on_complete immediately") {
        auto snk = flow::make_auto_observer<int>();
        ctx->make_observable()
          .zip_with([](int x, int y, int z) { return x + y + z; },
                    ctx->make_observable().repeat(11),
                    ctx->make_observable().repeat(22),
                    ctx->make_observable().empty<int>())
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK(snk->buf.empty());
        CHECK_EQ(snk->state, flow::observer_state::completed);
      }
    }
  }
}

SCENARIO("zip_with aborts if an input emits an error") {
  GIVEN("two observables, one of them emits an error after some items") {
    WHEN("merging them with zip_with") {
      THEN("the observer receives all items up to the error") {
        auto obs = ctx->make_observable();
        auto snk = flow::make_auto_observer<int>();
        obs //
          .iota(1)
          .take(3)
          .concat(obs.fail<int>(sec::runtime_error))
          .zip_with([](int x, int y) { return x + y; }, obs.iota(1).take(10))
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK(snk->aborted());
        CHECK_EQ(snk->buf, std::vector<int>({2, 4, 6}));
      }
    }
  }
  GIVEN("two observables, one of them emits an error immediately") {
    WHEN("merging them with zip_with") {
      THEN("the observer only receives on_error") {
        auto obs = ctx->make_observable();
        auto snk = flow::make_auto_observer<int>();
        obs //
          .iota(1)
          .take(3)
          .zip_with([](int x, int y) { return x + y; },
                    obs.fail<int>(sec::runtime_error))
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK(snk->aborted());
        CHECK(snk->buf.empty());
      }
    }
  }
}

SCENARIO("zip_with on an invalid observable produces an invalid observable") {
  GIVEN("a default-constructed (invalid) observable") {
    WHEN("calling zip_with on it") {
      THEN("the result is another invalid observable") {
        auto obs = ctx->make_observable();
        auto snk = flow::make_auto_observer<int>();
        flow::observable<int>{}
          .zip_with([](int x, int y) { return x + y; }, obs.iota(1).take(10))
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK(snk->aborted());
        CHECK(snk->buf.empty());
      }
    }
  }
  GIVEN("a valid observable") {
    WHEN("calling zip_with on it with an invalid observable") {
      THEN("the result is another invalid observable") {
        auto obs = ctx->make_observable();
        auto snk = flow::make_auto_observer<int>();
        obs //
          .iota(1)
          .take(10)
          .zip_with([](int x, int y) { return x + y; }, flow::observable<int>{})
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK(snk->aborted());
        CHECK(snk->buf.empty());
      }
    }
  }
}

SCENARIO("zip_with operators can be disposed at any time") {
  GIVEN("a zip_with operator that produces some items") {
    WHEN("calling dispose before requesting any items") {
      THEN("the observer never receives any item") {
        auto obs = ctx->make_observable();
        auto snk = flow::make_passive_observer<int>();
        auto sub = //
          obs      //
            .iota(1)
            .take(10)
            .zip_with([](int x, int y) { return x + y; }, obs.iota(1))
            .subscribe(snk->as_observer());
        CHECK(!sub.disposed());
        sub.dispose();
        ctx->run();
        CHECK(snk->completed());
      }
    }
    WHEN("calling dispose in on_subscribe") {
      THEN("the observer receives no item") {
        auto obs = ctx->make_observable();
        auto snk = flow::make_canceling_observer<int>();
        obs //
          .iota(1)
          .take(10)
          .zip_with([](int x, int y) { return x + y; }, obs.iota(1))
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK_EQ(snk->on_next_calls, 0);
      }
    }
    WHEN("calling dispose in on_next") {
      THEN("the observer receives no additional item") {
        auto obs = ctx->make_observable();
        auto snk = flow::make_canceling_observer<int>(true);
        obs //
          .iota(1)
          .take(10)
          .zip_with([](int x, int y) { return x + y; }, obs.iota(1))
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK_EQ(snk->on_next_calls, 1);
      }
    }
  }
}

SCENARIO("observers may request from zip_with operators before on_subscribe") {
  GIVEN("a zip_with operator with two inputs") {
    WHEN("the observer calls request before the inputs call on_subscribe") {
      THEN("the observer receives the item") {
        using flow::op::zip_index;
        auto snk = flow::make_passive_observer<int>();
        auto grd = make_unsubscribe_guard(snk);
        auto uut = make_zip_with_sub([](int, int) { return 0; },
                                     snk->as_observer(),
                                     flow::make_nil_observable<int>(ctx.get()),
                                     flow::make_nil_observable<int>(ctx.get()));
        snk->request(128);
        auto sub1 = flow::make_passive_subscription();
        auto sub2 = flow::make_passive_subscription();
        uut->fwd_on_subscribe(zip_index<0>{}, flow::subscription{sub1});
        uut->fwd_on_subscribe(zip_index<1>{}, flow::subscription{sub2});
        CHECK_EQ(sub1->demand, 128u);
        CHECK_EQ(sub2->demand, 128u);
      }
    }
  }
}

SCENARIO("the zip_with operators disposes unexpected subscriptions") {
  GIVEN("a zip_with operator with two inputs") {
    WHEN("on_subscribe is called twice for the same input") {
      THEN("the operator disposes the subscription") {
        using flow::op::zip_index;
        auto snk = flow::make_passive_observer<int>();
        auto grd = make_unsubscribe_guard(snk);
        auto uut = make_zip_with_sub([](int, int) { return 0; },
                                     snk->as_observer(),
                                     flow::make_nil_observable<int>(ctx.get()),
                                     flow::make_nil_observable<int>(ctx.get()));
        auto sub1 = flow::make_passive_subscription();
        auto sub2 = flow::make_passive_subscription();
        uut->fwd_on_subscribe(zip_index<0>{}, flow::subscription{sub1});
        uut->fwd_on_subscribe(zip_index<0>{}, flow::subscription{sub2});
        CHECK(!sub1->disposed());
        CHECK(sub2->disposed());
      }
    }
  }
}

END_FIXTURE_SCOPE()
