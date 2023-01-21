// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.broadcaster

#include "caf/flow/observable.hpp"

#include "core-test.hpp"

#include "caf/flow/observable_builder.hpp"
#include "caf/flow/scoped_coordinator.hpp"

using namespace caf;

namespace {

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("a broadcaster pushes items to single subscribers") {
  GIVEN("a broadcaster with one source and one sink") {
    auto uut = flow::make_broadcaster_impl<int>(ctx.get());
    auto src = flow::make_passive_observable<int>(ctx.get());
    auto snk = flow::make_passive_observer<int>();
    src->subscribe(uut->as_observer());
    uut->subscribe(snk->as_observer());
    WHEN("the source emits 10 items") {
      THEN("the broadcaster forwards them to its sink") {
        snk->sub.request(13);
        ctx->run();
        CHECK_EQ(src->demand, 13u);
        snk->sub.request(7);
        ctx->run();
        CHECK_EQ(src->demand, 20u);
        auto inputs = std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        src->push(make_span(inputs));
        CHECK_EQ(src->demand, 10u);
        CHECK_EQ(uut->buffered(), 0u);
        CHECK_EQ(snk->buf, std::vector<int>({1, 2, 3, 4, 5, 6, 7, 8, 9, 10}));
        src->complete();
        ctx->run();
      }
    }
  }
}

SCENARIO("a broadcaster pushes items to all subscribers at the same time") {
  GIVEN("a broadcaster with one source and three sinks") {
    auto uut = flow::make_broadcaster_impl<int>(ctx.get());
    auto src = flow::make_passive_observable<int>(ctx.get());
    auto snk1 = flow::make_passive_observer<int>();
    auto snk2 = flow::make_passive_observer<int>();
    auto snk3 = flow::make_passive_observer<int>();
    src->subscribe(uut->as_observer());
    uut->subscribe(snk1->as_observer());
    uut->subscribe(snk2->as_observer());
    uut->subscribe(snk3->as_observer());
    WHEN("the source emits 10 items") {
      THEN("the broadcaster forwards them to all sinks") {
        snk1->sub.request(13);
        ctx->run();
        CHECK_EQ(src->demand, 13u);
        snk2->sub.request(7);
        ctx->run();
        CHECK_EQ(src->demand, 13u);
        snk3->sub.request(21);
        ctx->run();
        CHECK_EQ(src->demand, 21u);
        auto inputs = std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        src->push(make_span(inputs));
        CHECK_EQ(src->demand, 11u);
        CHECK_EQ(uut->buffered(), 3u);
        CHECK_EQ(snk1->buf, std::vector<int>({1, 2, 3, 4, 5, 6, 7}));
        CHECK_EQ(snk2->buf, std::vector<int>({1, 2, 3, 4, 5, 6, 7}));
        CHECK_EQ(snk3->buf, std::vector<int>({1, 2, 3, 4, 5, 6, 7}));
        snk2->sub.request(7);
        ctx->run();
        CHECK_EQ(src->demand, 11u);
        CHECK_EQ(uut->buffered(), 0u);
        CHECK_EQ(snk1->buf, inputs);
        CHECK_EQ(snk2->buf, inputs);
        CHECK_EQ(snk3->buf, inputs);
        snk2->sub.request(14);
        ctx->run();
        CHECK_EQ(src->demand, 18u);
        src->complete();
        ctx->run();
      }
    }
  }
}

SCENARIO("a broadcaster emits values before propagating completion") {
  GIVEN("a broadcaster with one source and three sinks") {
    auto uut = flow::make_broadcaster_impl<int>(ctx.get());
    auto src = flow::make_passive_observable<int>(ctx.get());
    auto snk1 = flow::make_passive_observer<int>();
    auto snk2 = flow::make_passive_observer<int>();
    auto snk3 = flow::make_passive_observer<int>();
    src->subscribe(uut->as_observer());
    uut->subscribe(snk1->as_observer());
    uut->subscribe(snk2->as_observer());
    uut->subscribe(snk3->as_observer());
    WHEN("the source emits 10 items and then signals completion") {
      THEN("the broadcaster forwards all values before signaling an error") {
        snk1->sub.request(13);
        snk2->sub.request(7);
        snk3->sub.request(21);
        ctx->run();
        CHECK_EQ(src->demand, 21u);
        auto inputs = std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        src->push(make_span(inputs));
        src->complete();
        CHECK_EQ(uut->buffered(), 3u);
        CHECK_EQ(snk1->buf, std::vector<int>({1, 2, 3, 4, 5, 6, 7}));
        CHECK_EQ(snk2->buf, std::vector<int>({1, 2, 3, 4, 5, 6, 7}));
        CHECK_EQ(snk3->buf, std::vector<int>({1, 2, 3, 4, 5, 6, 7}));
        CHECK_EQ(uut->state(), flow::observable_state::completing);
        CHECK_EQ(snk1->state, flow::observer_state::subscribed);
        CHECK_EQ(snk2->state, flow::observer_state::subscribed);
        CHECK_EQ(snk3->state, flow::observer_state::subscribed);
        snk2->sub.request(7);
        ctx->run();
        CHECK_EQ(snk1->buf, inputs);
        CHECK_EQ(snk2->buf, inputs);
        CHECK_EQ(snk3->buf, inputs);
        CHECK_EQ(uut->state(), flow::observable_state::completed);
        CHECK_EQ(snk1->state, flow::observer_state::completed);
        CHECK_EQ(snk2->state, flow::observer_state::completed);
        CHECK_EQ(snk3->state, flow::observer_state::completed);
      }
    }
  }
}

SCENARIO("a broadcaster emits values before propagating errors") {
  GIVEN("a broadcaster with one source and three sinks") {
    auto uut = flow::make_broadcaster_impl<int>(ctx.get());
    auto src = flow::make_passive_observable<int>(ctx.get());
    auto snk1 = flow::make_passive_observer<int>();
    auto snk2 = flow::make_passive_observer<int>();
    auto snk3 = flow::make_passive_observer<int>();
    src->subscribe(uut->as_observer());
    uut->subscribe(snk1->as_observer());
    uut->subscribe(snk2->as_observer());
    uut->subscribe(snk3->as_observer());
    WHEN("the source emits 10 items and then stops with an error") {
      THEN("the broadcaster forwards all values before signaling an error") {
        snk1->sub.request(13);
        snk2->sub.request(7);
        snk3->sub.request(21);
        ctx->run();
        CHECK_EQ(src->demand, 21u);
        auto inputs = std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        src->push(make_span(inputs));
        auto runtime_error = make_error(sec::runtime_error);
        src->abort(runtime_error);
        CHECK_EQ(uut->buffered(), 3u);
        CHECK_EQ(snk1->buf, std::vector<int>({1, 2, 3, 4, 5, 6, 7}));
        CHECK_EQ(snk2->buf, std::vector<int>({1, 2, 3, 4, 5, 6, 7}));
        CHECK_EQ(snk3->buf, std::vector<int>({1, 2, 3, 4, 5, 6, 7}));
        CHECK_EQ(uut->state(), flow::observable_state::completing);
        CHECK_EQ(uut->err(), runtime_error);
        CHECK_EQ(snk1->state, flow::observer_state::subscribed);
        CHECK_EQ(snk2->state, flow::observer_state::subscribed);
        CHECK_EQ(snk3->state, flow::observer_state::subscribed);
        snk2->sub.request(7);
        ctx->run();
        CHECK_EQ(snk1->buf, inputs);
        CHECK_EQ(snk2->buf, inputs);
        CHECK_EQ(snk3->buf, inputs);
        CHECK_EQ(uut->state(), flow::observable_state::aborted);
        CHECK_EQ(snk1->state, flow::observer_state::aborted);
        CHECK_EQ(snk2->state, flow::observer_state::aborted);
        CHECK_EQ(snk3->state, flow::observer_state::aborted);
        CHECK_EQ(uut->err(), runtime_error);
        CHECK_EQ(snk1->err, runtime_error);
        CHECK_EQ(snk2->err, runtime_error);
        CHECK_EQ(snk3->err, runtime_error);
      }
    }
  }
}

END_FIXTURE_SCOPE()
