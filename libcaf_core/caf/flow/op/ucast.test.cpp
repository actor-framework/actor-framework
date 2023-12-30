// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/op/ucast.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

using namespace caf;

namespace {

using int_ucast = flow::op::ucast<int>;

using int_ucast_ptr = intrusive_ptr<int_ucast>;

struct fixture : test::fixture::flow {
  int_ucast_ptr make_ucast() {
    return make_counted<int_ucast>(coordinator());
  }
};

WITH_FIXTURE(fixture) {

SCENARIO("closed ucast operators appear empty") {
  GIVEN("a closed ucast operator") {
    WHEN("subscribing to it") {
      THEN("the observer receives an on_complete event") {
        using snk_t = flow::auto_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_ucast();
        uut->close();
        uut->subscribe(snk->as_observer());
        run_flows();
        check(snk->completed());
        uut->close();
        run_flows();
      }
    }
  }
}

SCENARIO("aborted ucast operators fail when subscribed") {
  GIVEN("an aborted ucast operator") {
    WHEN("subscribing to it") {
      THEN("the observer receives an on_error event") {
        using snk_t = flow::auto_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_ucast();
        uut->abort(sec::runtime_error);
        uut->subscribe(snk->as_observer());
        run_flows();
        check(snk->aborted());
        uut->close();
        run_flows();
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
        auto o1 = coordinator()->add_child(std::in_place_type<snk_t>);
        auto o2 = coordinator()->add_child(std::in_place_type<snk_t>);
        auto sub1 = uut->subscribe(o1->as_observer());
        auto sub2 = uut->subscribe(o2->as_observer());
        check(o1->subscribed());
        check(o2->aborted());
        uut->close();
        run_flows();
      }
    }
  }
}

SCENARIO("observers may cancel ucast subscriptions at any time") {
  GIVEN("a ucast operator") {
    WHEN("the observer disposes its subscription in on_next") {
      THEN("no further items arrive") {
        using snk_t = flow::canceling_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>, true);
        auto uut = make_ucast();
        auto sub = uut->subscribe(snk->as_observer());
        check(!sub.disposed());
        uut->push(1);
        uut->push(2);
        run_flows();
        check(sub.disposed());
        check_eq(snk->on_next_calls, 1);
        uut->close();
        run_flows();
      }
    }
  }
}

SCENARIO("ucast operators deliver pending items before raising errors") {
  GIVEN("a ucast operator with pending items") {
    WHEN("an error event occurs") {
      THEN("the operator still delivers the pending items first") {
        using snk_t = flow::auto_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_ucast();
        uut->subscribe(snk->as_observer());
        uut->push(1);
        uut->push(2);
        uut->abort(sec::runtime_error);
        run_flows();
        check(snk->aborted());
        check_eq(snk->buf, std::vector<int>({1, 2}));
        uut->close();
        run_flows();
      }
    }
  }
}

SCENARIO("requesting from disposed ucast operators is a no-op") {
  GIVEN("a ucast operator with a disposed subscription") {
    WHEN("calling request() on the subscription") {
      THEN("the demand is ignored") {
        using snk_t = flow::canceling_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>, true);
        auto uut = make_ucast();
        auto sub = uut->subscribe(snk->as_observer());
        check(!sub.disposed());
        uut->push(1);
        uut->push(2);
        run_flows();
        check(sub.disposed());
        dynamic_cast<caf::flow::subscription::impl*>(sub.ptr())->request(42);
        run_flows();
        check(sub.disposed());
        check_eq(snk->on_next_calls, 1);
        uut->close();
        run_flows();
      }
    }
  }
}

} // WITH_FIXTURE(fixture)

} // namespace
