// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.op.ucast

#include "caf/flow/op/ucast.hpp"

#include "caf/flow/observable.hpp"
#include "caf/flow/scoped_coordinator.hpp"

#include "core-test.hpp"

using namespace caf;

namespace {

using int_ucast = flow::op::ucast<int>;

using int_ucast_ptr = intrusive_ptr<int_ucast>;

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();

  int_ucast_ptr make_ucast() {
    return make_counted<int_ucast>(ctx.get());
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("closed ucast operators appear empty") {
  GIVEN("a closed ucast operator") {
    WHEN("subscribing to it") {
      THEN("the observer receives an on_complete event") {
        using snk_t = flow::auto_observer<int>;
        auto snk = ctx->add_child(std::in_place_type<snk_t>);
        auto uut = make_ucast();
        uut->close();
        uut->subscribe(snk->as_observer());
        ctx->run();
        CHECK(snk->completed());
      }
    }
  }
}

SCENARIO("aborted ucast operators fail when subscribed") {
  GIVEN("an aborted ucast operator") {
    WHEN("subscribing to it") {
      THEN("the observer receives an on_error event") {
        using snk_t = flow::auto_observer<int>;
        auto snk = ctx->add_child(std::in_place_type<snk_t>);
        auto uut = make_ucast();
        uut->abort(sec::runtime_error);
        uut->subscribe(snk->as_observer());
        ctx->run();
        CHECK(snk->aborted());
      }
    }
  }
}

SCENARIO("ucast operators may only be subscribed to once") {
  GIVEN("a ucast operator") {
    WHEN("two observers subscribe to it") {
      THEN("the second subscription fails") {
        using snk_t = flow::passive_observer<int>;
        auto uut = make_ucast();
        auto o1 = ctx->add_child(std::in_place_type<snk_t>);
        auto o2 = ctx->add_child(std::in_place_type<snk_t>);
        auto grd = make_unsubscribe_guard(o1, o2);
        auto sub1 = uut->subscribe(o1->as_observer());
        auto sub2 = uut->subscribe(o2->as_observer());
        CHECK(o1->subscribed());
        CHECK(o2->aborted());
      }
    }
  }
}

SCENARIO("observers may cancel ucast subscriptions at any time") {
  GIVEN("a ucast operator") {
    WHEN("the observer disposes its subscription in on_next") {
      THEN("no further items arrive") {
        using snk_t = flow::canceling_observer<int>;
        auto snk = ctx->add_child(std::in_place_type<snk_t>, true);
        auto uut = make_ucast();
        auto sub = uut->subscribe(snk->as_observer());
        CHECK(!sub.disposed());
        uut->push(1);
        uut->push(2);
        ctx->run();
        CHECK(sub.disposed());
        CHECK_EQ(snk->on_next_calls, 1);
      }
    }
  }
}

SCENARIO("ucast operators deliver pending items before raising errors") {
  GIVEN("a ucast operator with pending items") {
    WHEN("an error event occurs") {
      THEN("the operator still delivers the pending items first") {
        using snk_t = flow::auto_observer<int>;
        auto snk = ctx->add_child(std::in_place_type<snk_t>);
        auto uut = make_ucast();
        uut->subscribe(snk->as_observer());
        uut->push(1);
        uut->push(2);
        uut->abort(sec::runtime_error);
        ctx->run();
        CHECK(snk->aborted());
        CHECK_EQ(snk->buf, std::vector<int>({1, 2}));
      }
    }
  }
}

SCENARIO("requesting from disposed ucast operators is a no-op") {
  GIVEN("a ucast operator with a disposed subscription") {
    WHEN("calling request() on the subscription") {
      THEN("the demand is ignored") {
        using snk_t = flow::canceling_observer<int>;
        auto snk = ctx->add_child(std::in_place_type<snk_t>, true);
        auto uut = make_ucast();
        auto sub = uut->subscribe(snk->as_observer());
        CHECK(!sub.disposed());
        uut->push(1);
        uut->push(2);
        ctx->run();
        CHECK(sub.disposed());
        dynamic_cast<flow::subscription::impl*>(sub.ptr())->request(42);
        ctx->run();
        CHECK(sub.disposed());
        CHECK_EQ(snk->on_next_calls, 1);
      }
    }
  }
}

END_FIXTURE_SCOPE()
