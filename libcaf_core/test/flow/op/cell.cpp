// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.op.cell

#include "caf/flow/op/cell.hpp"

#include "core-test.hpp"

#include "caf/flow/observable.hpp"
#include "caf/flow/scoped_coordinator.hpp"

using namespace caf;

namespace {

using int_cell = flow::op::cell<int>;

using int_cell_ptr = intrusive_ptr<int_cell>;

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();

  int_cell_ptr make_cell() {
    return make_counted<int_cell>(ctx.get());
  }

  flow::observable<int> lift(int_cell_ptr cell) {
    return flow::observable<int>{cell};
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("a null cell emits zero items") {
  GIVEN("an integer cell with an observer") {
    WHEN("calling set_null on the cell") {
      THEN("the observer receives the completed event") {
        auto uut = make_cell();
        auto snk = flow::make_passive_observer<int>();
        lift(uut).subscribe(snk->as_observer());
        REQUIRE(snk->subscribed());
        snk->sub.request(128);
        ctx->run();
        REQUIRE(snk->subscribed());
        uut->set_null();
        ctx->run();
        CHECK(snk->completed());
        CHECK(snk->buf.empty());
      }
    }
  }
  GIVEN("an integer cell without on bserver") {
    WHEN("calling set_null on the cell") {
      THEN("observers receive completed events immediately after subscribing") {
        auto uut = make_cell();
        uut->set_null();
        auto snk = flow::make_passive_observer<int>();
        lift(uut).subscribe(snk->as_observer());
        REQUIRE(snk->subscribed());
        snk->sub.request(128);
        ctx->run();
        CHECK(snk->completed());
        CHECK(snk->buf.empty());
      }
    }
  }
}

SCENARIO("a cell with a value emits exactly one item") {
  GIVEN("an integer cell with an observer") {
    WHEN("calling set_value on the cell") {
      THEN("the observer receives on_next and then on_complete") {
        auto uut = make_cell();
        auto snk = flow::make_passive_observer<int>();
        lift(uut).subscribe(snk->as_observer());
        REQUIRE(snk->subscribed());
        snk->sub.request(128);
        ctx->run();
        REQUIRE(snk->subscribed());
        uut->set_value(42);
        ctx->run();
        CHECK(snk->completed());
        CHECK_EQ(snk->buf, std::vector<int>{42});
      }
    }
    WHEN("disposing the subscription before calling set_value on the cell") {
      THEN("the observer does not receive the item") {
        auto uut = make_cell();
        auto snk = flow::make_passive_observer<int>();
        lift(uut).subscribe(snk->as_observer());
        REQUIRE(snk->subscribed());
        snk->request(128);
        ctx->run();
        // Normally, we'd call snk->unsubscribe() here. However, that nulls the
        // subscription. We want the sub.disposed() call below actually call
        // cell_sub::disposed() to have coverage on that member function.
        snk->sub.ptr()->dispose();
        snk->state = flow::observer_state::idle;
        ctx->run();
        CHECK(snk->sub.disposed());
        CHECK(snk->idle());
        uut->set_value(42);
        ctx->run();
        CHECK(snk->idle());
        CHECK(snk->buf.empty());
      }
    }
  }
  GIVEN("an integer cell without on bserver") {
    WHEN("calling set_null on the cell") {
      THEN("the observer receives on_next and then on_complete immediately") {
        auto uut = make_cell();
        uut->set_value(42);
        auto snk = flow::make_passive_observer<int>();
        lift(uut).subscribe(snk->as_observer());
        REQUIRE(snk->subscribed());
        snk->sub.request(128);
        ctx->run();
        CHECK(snk->completed());
        CHECK_EQ(snk->buf, std::vector<int>{42});
      }
    }
  }
}

SCENARIO("a failed cell emits zero item") {
  GIVEN("an integer cell with an observer") {
    WHEN("calling set_error on the cell") {
      THEN("the observer receives on_error") {
        auto uut = make_cell();
        auto snk = flow::make_passive_observer<int>();
        lift(uut).subscribe(snk->as_observer());
        REQUIRE(snk->subscribed());
        snk->sub.request(128);
        ctx->run();
        REQUIRE(snk->subscribed());
        uut->set_error(sec::runtime_error);
        ctx->run();
        CHECK(snk->aborted());
        CHECK(snk->buf.empty());
        CHECK_EQ(snk->err, sec::runtime_error);
      }
    }
  }
  GIVEN("an integer cell without on bserver") {
    WHEN("calling set_error on the cell") {
      THEN("the observer receives on_error immediately when subscribing") {
        auto uut = make_cell();
        uut->set_error(sec::runtime_error);
        auto snk = flow::make_passive_observer<int>();
        lift(uut).subscribe(snk->as_observer());
        REQUIRE(snk->subscribed());
        snk->sub.request(128);
        ctx->run();
        CHECK(snk->aborted());
        CHECK(snk->buf.empty());
        CHECK_EQ(snk->err, sec::runtime_error);
      }
    }
  }
}

END_FIXTURE_SCOPE()
