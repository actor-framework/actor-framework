// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/flow/op/cell.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"

using namespace caf;

namespace {

using int_cell = flow::op::cell<int>;

using int_cell_ptr = intrusive_ptr<int_cell>;

struct fixture : test::fixture::deterministic, test::fixture::flow {
  int_cell_ptr make_cell() {
    return make_counted<int_cell>(coordinator());
  }

  caf::flow::observable<int> lift(int_cell_ptr cell) {
    return caf::flow::observable<int>{cell};
  }
};

WITH_FIXTURE(fixture) {

SCENARIO("a null cell emits zero items") {
  GIVEN("an integer cell with an observer") {
    WHEN("calling set_null on the cell") {
      THEN("the observer receives the completed event") {
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_cell();
        lift(uut).subscribe(snk->as_observer());
        require(snk->subscribed());
        snk->sub.request(128);
        run_flows();
        require(snk->subscribed());
        uut->set_null();
        run_flows();
        check(snk->completed());
        check(snk->buf.empty());
      }
    }
  }
  GIVEN("an integer cell without on bserver") {
    WHEN("calling set_null on the cell") {
      THEN("observers receive completed events immediately after subscribing") {
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_cell();
        uut->set_null();
        lift(uut).subscribe(snk->as_observer());
        require(snk->subscribed());
        snk->sub.request(128);
        run_flows();
        check(snk->completed());
        check(snk->buf.empty());
      }
    }
  }
}

SCENARIO("a cell with a value emits exactly one item") {
  GIVEN("an integer cell with an observer") {
    WHEN("calling set_value on the cell") {
      THEN("the observer receives on_next and then on_complete") {
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_cell();
        lift(uut).subscribe(snk->as_observer());
        require(snk->subscribed());
        snk->sub.request(128);
        run_flows();
        require(snk->subscribed());
        uut->set_value(42);
        run_flows();
        check(snk->completed());
        check_eq(snk->buf, std::vector<int>{42});
      }
    }
    WHEN("disposing the subscription before calling set_value on the cell") {
      THEN("the observer does not receive the item") {
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_cell();
        lift(uut).subscribe(snk->as_observer());
        require(snk->subscribed());
        snk->request(128);
        run_flows();
        // Normally, we'd call snk->unsubscribe() here. However, that nulls the
        // subscription. We want the sub.disposed() call below actually call
        // cell_sub::disposed() to have coverage on that member function.
        snk->sub.ptr()->cancel();
        snk->state = flow::observer_state::idle;
        run_flows();
        check(snk->idle());
        uut->set_value(42);
        run_flows();
        check(snk->idle());
        check(snk->buf.empty());
      }
    }
  }
  GIVEN("an integer cell without on bserver") {
    WHEN("calling set_null on the cell") {
      THEN("the observer receives on_next and then on_complete immediately") {
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_cell();
        uut->set_value(42);
        lift(uut).subscribe(snk->as_observer());
        require(snk->subscribed());
        snk->sub.request(128);
        run_flows();
        check(snk->completed());
        check_eq(snk->buf, std::vector<int>{42});
      }
    }
  }
}

SCENARIO("a failed cell emits zero item") {
  GIVEN("an integer cell with an observer") {
    WHEN("calling set_error on the cell") {
      THEN("the observer receives on_error") {
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_cell();
        lift(uut).subscribe(snk->as_observer());
        require(snk->subscribed());
        snk->sub.request(128);
        run_flows();
        require(snk->subscribed());
        uut->set_error(sec::runtime_error);
        run_flows();
        check(snk->aborted());
        check(snk->buf.empty());
        check_eq(snk->err, sec::runtime_error);
      }
    }
  }
  GIVEN("an integer cell without on bserver") {
    WHEN("calling set_error on the cell") {
      THEN("the observer receives on_error immediately when subscribing") {
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_cell();
        uut->set_error(sec::runtime_error);
        lift(uut).subscribe(snk->as_observer());
        require(snk->subscribed());
        snk->sub.request(128);
        run_flows();
        check(snk->aborted());
        check(snk->buf.empty());
        check_eq(snk->err, sec::runtime_error);
      }
    }
  }
}

} // WITH_FIXTURE(fixture)

} // namespace
