// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/flow/op/cell.hpp"

using std::vector;

using namespace caf;
using namespace caf::flow;

namespace {

template <class... Ts>
auto subscribe_all(Ts... xs) {
  auto snks = std::vector<flow::observer<int>>{flow::observer<int>{xs}...};
  return [snks](auto src) {
    for (auto snk : snks)
      src.subscribe(snk);
    return src;
  };
}

auto make_hot_generator() {
  auto n = std::make_shared<int>(0);
  auto fn = [n] {
    std::optional<int> result;
    if (*n < 10)
      result = ++*n;
    return result;
  };
  return std::make_pair(n, fn);
}

} // namespace

WITH_FIXTURE(test::fixture::flow) {

SCENARIO("publish creates a connectable observable") {
  using snk_t = flow::auto_observer<int>;
  GIVEN("a connectable with a hot generator") {
    WHEN("connecting without any subscriber") {
      THEN("all items get dropped") {
        auto [n, fn] = make_hot_generator();
        make_observable().from_callable(fn).publish().connect();
        run_flows();
        check_eq(*n, 10);
      }
    }
    WHEN("connecting after two observers have subscribed") {
      THEN("each observer receives all items from the generator") {
        auto [n, fn] = make_hot_generator();
        auto snk1 = coordinator()->add_child(std::in_place_type<snk_t>);
        auto snk2 = coordinator()->add_child(std::in_place_type<snk_t>);
        make_observable()
          .from_callable(fn)
          .publish()
          .compose(subscribe_all(snk1, snk2))
          .connect();
        run_flows();
        check_eq(*n, 10);
        check(snk1->completed());
        check_eq(snk1->buf, vector{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        check(snk2->completed());
        check_eq(snk2->buf, vector{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
      }
    }
    WHEN("adding two observers with auto_connect(2)") {
      THEN("each observer receives all items from the generator") {
        auto [n, fn] = make_hot_generator();
        auto snk1 = coordinator()->add_child(std::in_place_type<snk_t>);
        auto snk2 = coordinator()->add_child(std::in_place_type<snk_t>);
        make_observable().from_callable(fn).publish().auto_connect(2).compose(
          subscribe_all(snk1, snk2));
        run_flows();
        check_eq(*n, 10);
        check(snk1->completed());
        check_eq(snk1->buf, vector{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        check(snk2->completed());
        check_eq(snk2->buf, vector{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
      }
    }
    WHEN("adding two observers with share(2)") {
      THEN("each observer receives all items from the generator") {
        auto [n, fn] = make_hot_generator();
        auto snk1 = coordinator()->add_child(std::in_place_type<snk_t>);
        auto snk2 = coordinator()->add_child(std::in_place_type<snk_t>);
        make_observable()
          .from_callable(fn) //
          .share(2)
          .compose(subscribe_all(snk1, snk2));
        run_flows();
        check_eq(*n, 10);
        check(snk1->completed());
        check_eq(snk1->buf, vector{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        check(snk2->completed());
        check_eq(snk2->buf, vector{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
      }
    }
  }
}

SCENARIO("connectable observables forward errors") {
  using snk_t = flow::auto_observer<int>;
  GIVEN("a connectable with a cell and two subscribers") {
    WHEN("the cell fails") {
      THEN("all subscribers receive the error") {
        auto cell = make_counted<op::cell<int>>(coordinator());
        auto snk1 = coordinator()->add_child(std::in_place_type<snk_t>);
        auto snk2 = coordinator()->add_child(std::in_place_type<snk_t>);
        observable<int>{cell}.share(2).compose(subscribe_all(snk1, snk2));
        run_flows();
        check(snk1->subscribed());
        check(snk2->subscribed());
        cell->set_error(sec::runtime_error);
        run_flows();
        check(snk1->aborted());
        check(snk2->aborted());
      }
    }
  }
  GIVEN("an already failed connectable") {
    WHEN("subscribing to it") {
      THEN("the subscribers receive the error immediately") {
        auto cell = make_counted<op::cell<int>>(coordinator());
        auto conn = observable<int>{cell}.share();
        cell->set_error(sec::runtime_error);
        // First subscriber to trigger subscription to the cell.
        auto snk1 = coordinator()->add_child(std::in_place_type<snk_t>);
        conn.subscribe(snk1->as_observer());
        run_flows();
        check(snk1->aborted());
        // After this point, new subscribers should be aborted right away.
        auto snk2 = coordinator()->add_child(std::in_place_type<snk_t>);
        auto sub = conn.subscribe(snk2->as_observer());
        check(snk2->aborted());
        run_flows();
      }
    }
  }
}

SCENARIO("observers that dispose their subscription do not affect others") {
  using snk_t = flow::passive_observer<int>;
  GIVEN("a connectable with two subscribers") {
    WHEN("one of the subscribers disposes its subscription") {
      THEN("the other subscriber still receives all data") {
        using impl_t = op::publish<int>;
        auto snk1 = coordinator()->add_child(std::in_place_type<snk_t>);
        auto snk2 = coordinator()->add_child(std::in_place_type<snk_t>);
        // auto grd = make_unsubscribe_guard(snk1, snk2);
        auto iota = make_observable().iota(1).take(12).as_observable();
        auto uut = make_counted<impl_t>(coordinator(), iota.pimpl(), 5);
        auto sub1 = uut->subscribe(snk1->as_observer());
        auto sub2 = uut->subscribe(snk2->as_observer());
        uut->connect();
        run_flows();
        snk1->request(7);
        snk2->request(3);
        run_flows();
        check_eq(snk1->buf, vector{1, 2, 3, 4, 5, 6, 7});
        check_eq(snk2->buf, vector{1, 2, 3});
        snk2->sub.cancel();
        run_flows();
        snk1->request(42);
        run_flows();
        check_eq(snk1->buf, vector{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
        sub1.dispose();
        sub2.dispose();
        run_flows();
        check(sub1.disposed());
        check(sub2.disposed());
      }
    }
  }
}

SCENARIO("publishers with auto_disconnect auto-dispose their subscription") {
  using snk_t = flow::passive_observer<int>;
  GIVEN("a connectable with two subscribers") {
    WHEN("both subscribers drop out and auto_disconnect is enabled") {
      THEN("the publisher becomes disconnected") {
        using impl_t = op::publish<int>;
        auto snk1 = coordinator()->add_child(std::in_place_type<snk_t>);
        auto snk2 = coordinator()->add_child(std::in_place_type<snk_t>);
        // auto grd = make_unsubscribe_guard(snk1, snk2);
        auto iota = make_observable().iota(1).take(12).as_observable();
        auto uut = make_counted<impl_t>(coordinator(), iota.pimpl(), 5);
        auto sub1 = uut->subscribe(snk1->as_observer());
        auto sub2 = uut->subscribe(snk2->as_observer());
        uut->auto_disconnect(true);
        uut->connect();
        check(uut->connected());
        run_flows();
        snk1->request(7);
        snk2->request(3);
        run_flows();
        check_eq(snk1->buf, vector{1, 2, 3, 4, 5, 6, 7});
        check_eq(snk2->buf, vector{1, 2, 3});
        snk1->sub.cancel();
        snk2->sub.cancel();
        run_flows();
        check(!uut->connected());
        sub1.dispose();
        sub2.dispose();
        run_flows();
        check(sub1.disposed());
        check(sub2.disposed());
      }
    }
  }
}

SCENARIO("publishers dispose unexpected subscriptions") {
  using snk_t = flow::passive_observer<int>;
  GIVEN("an initialized publish operator") {
    WHEN("calling on_subscribe with unexpected subscriptions") {
      THEN("the operator disposes them immediately") {
        using impl_t = op::publish<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto iota = make_observable().iota(1).take(12).as_observable();
        auto uut = make_counted<impl_t>(coordinator(), iota.pimpl());
        uut->subscribe(snk->as_observer());
        uut->connect();
        auto sub = coordinator()->add_child(
          std::in_place_type<op::never_sub<int>>, snk->as_observer());
        uut->on_subscribe(caf::flow::subscription{sub});
        check(sub->disposed());
        run_flows();
        snk->sub.cancel();
        run_flows();
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::flow)
