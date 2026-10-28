// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/flow/op/publish.hpp"

#include "caf/flow/backpressure_overflow_strategy.hpp"
#include "caf/flow/multicaster.hpp"
#include "caf/flow/op/cell.hpp"
#include "caf/sec.hpp"

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

using overflow_strategy = caf::flow::backpressure_overflow_strategy;

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
        conn.subscribe(snk2->as_observer());
        run_flows();
        check(snk2->aborted());
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
        auto conn = uut->connect();
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

SCENARIO("publish gates the credit for its source on its slowest subscriber") {
  using snk_t = flow::passive_observer<int>;
  using impl_t = op::publish<int>;
  GIVEN("a publish operator with a buffer size of 5 and two subscribers") {
    WHEN("one subscriber fills its buffer and stops requesting items") {
      THEN("the operator stops requesting items from its source") {
        auto src = caf::flow::multicaster<int>{coordinator()};
        auto fast = coordinator()->add_child(std::in_place_type<snk_t>);
        auto slow = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_counted<impl_t>(coordinator(),
                                        src.as_observable().pimpl(), 5);
        auto sub1 = uut->subscribe(fast->as_observer());
        auto sub2 = uut->subscribe(slow->as_observer());
        auto conn = uut->connect();
        run_flows();
        check_eq(src.demand(), 5u);
        src.push({1, 2, 3, 4, 5});
        run_flows();
        check_eq(src.demand(), 0u);
        fast->request(100);
        run_flows();
        check_eq(fast->buf, vector{1, 2, 3, 4, 5});
        check_eq(slow->buf, vector<int>{});
        check_eq(uut->max_buffered(), 5u);
        check_eq(src.demand(), 0u);
        sub1.dispose();
        sub2.dispose();
        run_flows();
      }
    }
    WHEN("the slow subscriber consumes part of its buffer") {
      THEN("the operator requests exactly the freed-up capacity") {
        auto src = caf::flow::multicaster<int>{coordinator()};
        auto fast = coordinator()->add_child(std::in_place_type<snk_t>);
        auto slow = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_counted<impl_t>(coordinator(),
                                        src.as_observable().pimpl(), 5);
        auto sub1 = uut->subscribe(fast->as_observer());
        auto sub2 = uut->subscribe(slow->as_observer());
        auto conn = uut->connect();
        run_flows();
        src.push({1, 2, 3, 4, 5});
        fast->request(100);
        run_flows();
        slow->request(3);
        run_flows();
        check_eq(slow->buf, vector{1, 2, 3});
        check_eq(uut->max_buffered(), 2u);
        check_eq(src.demand(), 3u);
        sub1.dispose();
        sub2.dispose();
        run_flows();
      }
    }
    WHEN("the slow subscriber cancels its subscription") {
      THEN("the operator restores its full credit window") {
        auto src = caf::flow::multicaster<int>{coordinator()};
        auto fast = coordinator()->add_child(std::in_place_type<snk_t>);
        auto slow = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_counted<impl_t>(coordinator(),
                                        src.as_observable().pimpl(), 5);
        auto sub1 = uut->subscribe(fast->as_observer());
        auto sub2 = uut->subscribe(slow->as_observer());
        auto conn = uut->connect();
        run_flows();
        src.push({1, 2, 3, 4, 5});
        fast->request(100);
        run_flows();
        slow->sub.cancel();
        run_flows();
        check_eq(uut->observer_count(), 1u);
        check_eq(src.demand(), 5u);
        sub1.dispose();
        sub2.dispose();
        run_flows();
      }
    }
  }
}

SCENARIO("a subscriber that stops requesting items stalls the other "
         "subscribers") {
  using snk_t = flow::passive_observer<int>;
  using impl_t = op::publish<int>;
  GIVEN("a publish operator with a buffer size of 5 over a hot generator") {
    WHEN("one of two subscribers never requests any item") {
      THEN("the generator produces no more than one buffer worth of items") {
        auto [n, fn] = make_hot_generator();
        auto src = make_observable().from_callable(fn).as_observable();
        auto fast = coordinator()->add_child(std::in_place_type<snk_t>);
        auto slow = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_counted<impl_t>(coordinator(), src.pimpl(), 5);
        auto sub1 = uut->subscribe(fast->as_observer());
        auto sub2 = uut->subscribe(slow->as_observer());
        auto conn = uut->connect();
        run_flows();
        check_eq(*n, 5);
        fast->request(100);
        run_flows();
        check_eq(*n, 5);
        check_eq(fast->buf, vector{1, 2, 3, 4, 5});
        check(fast->subscribed());
        sub1.dispose();
        sub2.dispose();
        run_flows();
      }
    }
    WHEN("the slow subscriber consumes part of its buffer") {
      THEN("the generator resumes for exactly the freed-up capacity") {
        auto [n, fn] = make_hot_generator();
        auto src = make_observable().from_callable(fn).as_observable();
        auto fast = coordinator()->add_child(std::in_place_type<snk_t>);
        auto slow = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_counted<impl_t>(coordinator(), src.pimpl(), 5);
        auto sub1 = uut->subscribe(fast->as_observer());
        auto sub2 = uut->subscribe(slow->as_observer());
        auto conn = uut->connect();
        run_flows();
        fast->request(100);
        run_flows();
        slow->request(3);
        run_flows();
        check_eq(*n, 8);
        check_eq(slow->buf, vector{1, 2, 3});
        check_eq(fast->buf, vector{1, 2, 3, 4, 5, 6, 7, 8});
        sub1.dispose();
        sub2.dispose();
        run_flows();
      }
    }
    WHEN("the slow subscriber cancels its subscription") {
      THEN("the generator runs to completion") {
        auto [n, fn] = make_hot_generator();
        auto src = make_observable().from_callable(fn).as_observable();
        auto fast = coordinator()->add_child(std::in_place_type<snk_t>);
        auto slow = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_counted<impl_t>(coordinator(), src.pimpl(), 5);
        auto sub1 = uut->subscribe(fast->as_observer());
        auto sub2 = uut->subscribe(slow->as_observer());
        auto conn = uut->connect();
        run_flows();
        fast->request(100);
        run_flows();
        slow->sub.cancel();
        run_flows();
        check_eq(*n, 10);
        check_eq(fast->buf, vector{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        check(fast->completed());
        sub1.dispose();
        sub2.dispose();
        run_flows();
      }
    }
  }
}

SCENARIO("an overflow strategy keeps slow subscribers from stalling the flow") {
  using snk_t = flow::passive_observer<int>;
  using impl_t = op::publish<int>;
  GIVEN("a publish operator with the drop_newest strategy") {
    WHEN("one of two subscribers never requests any item") {
      THEN("the source completes and the slow subscriber keeps the oldest "
           "items") {
        auto [n, fn] = make_hot_generator();
        auto src = make_observable().from_callable(fn).as_observable();
        auto fast = coordinator()->add_child(std::in_place_type<snk_t>);
        auto slow = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_counted<impl_t>(coordinator(), src.pimpl(), 5,
                                        overflow_strategy::drop_newest);
        auto sub1 = uut->subscribe(fast->as_observer());
        auto sub2 = uut->subscribe(slow->as_observer());
        auto conn = uut->connect();
        run_flows();
        fast->request(100);
        run_flows();
        check_eq(*n, 10);
        check_eq(fast->buf, vector{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        check(fast->completed());
        slow->request(100);
        run_flows();
        check_eq(slow->buf, vector{1, 2, 3, 4, 5});
        check(slow->completed());
        sub1.dispose();
        sub2.dispose();
        run_flows();
      }
    }
  }
  GIVEN("a publish operator with the drop_oldest strategy") {
    WHEN("one of two subscribers never requests any item") {
      THEN("the source completes and the slow subscriber keeps the newest "
           "items") {
        auto [n, fn] = make_hot_generator();
        auto src = make_observable().from_callable(fn).as_observable();
        auto fast = coordinator()->add_child(std::in_place_type<snk_t>);
        auto slow = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_counted<impl_t>(coordinator(), src.pimpl(), 5,
                                        overflow_strategy::drop_oldest);
        auto sub1 = uut->subscribe(fast->as_observer());
        auto sub2 = uut->subscribe(slow->as_observer());
        auto conn = uut->connect();
        run_flows();
        fast->request(100);
        run_flows();
        check_eq(*n, 10);
        check_eq(fast->buf, vector{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        check(fast->completed());
        slow->request(100);
        run_flows();
        check_eq(slow->buf, vector{6, 7, 8, 9, 10});
        check(slow->completed());
        sub1.dispose();
        sub2.dispose();
        run_flows();
      }
    }
  }
  GIVEN("a publish operator with the fail strategy") {
    WHEN("one of two subscribers never requests any item") {
      THEN("only the slow subscriber receives sec::backpressure_overflow") {
        auto [n, fn] = make_hot_generator();
        auto src = make_observable().from_callable(fn).as_observable();
        auto fast = coordinator()->add_child(std::in_place_type<snk_t>);
        auto slow = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_counted<impl_t>(coordinator(), src.pimpl(), 5,
                                        overflow_strategy::fail);
        auto sub1 = uut->subscribe(fast->as_observer());
        auto sub2 = uut->subscribe(slow->as_observer());
        auto conn = uut->connect();
        run_flows();
        fast->request(100);
        run_flows();
        check(slow->aborted());
        check_eq(slow->err, sec::backpressure_overflow);
        check_eq(slow->buf, vector<int>{});
        check_eq(*n, 10);
        check_eq(fast->buf, vector{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        check(fast->completed());
        sub1.dispose();
        sub2.dispose();
        run_flows();
      }
    }
  }
}

SCENARIO("an overflow strategy is transparent for subscribers that keep up") {
  using snk_t = flow::auto_observer<int>;
  GIVEN("a connectable with an overflow strategy and two observers") {
    WHEN("both observers consume items as they arrive") {
      THEN("each observer receives all items from the generator") {
        for (auto strategy : {overflow_strategy::drop_newest,
                              overflow_strategy::drop_oldest,
                              overflow_strategy::fail}) {
          auto [n, fn] = make_hot_generator();
          auto snk1 = coordinator()->add_child(std::in_place_type<snk_t>);
          auto snk2 = coordinator()->add_child(std::in_place_type<snk_t>);
          make_observable()
            .from_callable(fn)
            .publish(strategy, 5)
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
    }
  }
}

SCENARIO("share accepts an overflow strategy") {
  GIVEN("a shared observable with the drop_oldest strategy") {
    WHEN("one subscriber consumes items and the other one does not") {
      THEN("the source completes and the slow subscriber keeps the newest "
           "items") {
        auto [n, fn] = make_hot_generator();
        auto fast = coordinator()->add_child(
          std::in_place_type<flow::auto_observer<int>>);
        auto slow = coordinator()->add_child(
          std::in_place_type<flow::passive_observer<int>>);
        make_observable()
          .from_callable(fn)
          .share(2, overflow_strategy::drop_oldest, 5)
          .compose(subscribe_all(fast, slow));
        run_flows();
        check_eq(*n, 10);
        check_eq(fast->buf, vector{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        check(fast->completed());
        slow->request(100);
        run_flows();
        check_eq(slow->buf, vector{6, 7, 8, 9, 10});
        check(slow->completed());
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
        auto conn = uut->connect();
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
