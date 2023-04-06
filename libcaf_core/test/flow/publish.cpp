// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.publish

#include "caf/flow/observer.hpp"

#include "core-test.hpp"

#include "caf/flow/observable_builder.hpp"
#include "caf/flow/op/cell.hpp"
#include "caf/flow/scoped_coordinator.hpp"

using namespace caf;

using caf::flow::make_observer;

namespace {

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();

  ~fixture() {
    ctx->run();
  }

  template <class... Ts>
  std::vector<int> ls(Ts... xs) {
    return std::vector<int>{xs...};
  }
};

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

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("publish creates a connectable observable") {
  GIVEN("a connectable with a hot generator") {
    WHEN("connecting without any subscriber") {
      THEN("all items get dropped") {
        auto [n, fn] = make_hot_generator();
        ctx->make_observable().from_callable(fn).publish().connect();
        ctx->run();
        CHECK_EQ(*n, 10);
      }
    }
    WHEN("connecting after two observers have subscribed") {
      THEN("each observer receives all items from the generator") {
        auto [n, fn] = make_hot_generator();
        auto snk1 = flow::make_auto_observer<int>();
        auto snk2 = flow::make_auto_observer<int>();
        ctx->make_observable()
          .from_callable(fn)
          .publish()
          .compose(subscribe_all(snk1, snk2))
          .connect();
        ctx->run();
        CHECK_EQ(*n, 10);
        CHECK(snk1->completed());
        CHECK_EQ(snk1->buf, ls(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
        CHECK(snk2->completed());
        CHECK_EQ(snk2->buf, ls(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
      }
    }
    WHEN("adding two observers with auto_connect(2)") {
      THEN("each observer receives all items from the generator") {
        auto [n, fn] = make_hot_generator();
        auto snk1 = flow::make_auto_observer<int>();
        auto snk2 = flow::make_auto_observer<int>();
        ctx->make_observable()
          .from_callable(fn)
          .publish()
          .auto_connect(2)
          .compose(subscribe_all(snk1, snk2));
        ctx->run();
        CHECK_EQ(*n, 10);
        CHECK(snk1->completed());
        CHECK_EQ(snk1->buf, ls(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
        CHECK(snk2->completed());
        CHECK_EQ(snk2->buf, ls(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
      }
    }
    WHEN("adding two observers with share(2)") {
      THEN("each observer receives all items from the generator") {
        auto [n, fn] = make_hot_generator();
        auto snk1 = flow::make_auto_observer<int>();
        auto snk2 = flow::make_auto_observer<int>();
        ctx->make_observable()
          .from_callable(fn) //
          .share(2)
          .compose(subscribe_all(snk1, snk2));
        ctx->run();
        CHECK_EQ(*n, 10);
        CHECK(snk1->completed());
        CHECK_EQ(snk1->buf, ls(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
        CHECK(snk2->completed());
        CHECK_EQ(snk2->buf, ls(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
      }
    }
  }
}

SCENARIO("connectable observables forward errors") {
  GIVEN("a connectable with a cell and two subscribers") {
    WHEN("the cell fails") {
      THEN("all subscribers receive the error") {
        auto cell = make_counted<flow::op::cell<int>>(ctx.get());
        auto snk1 = flow::make_auto_observer<int>();
        auto snk2 = flow::make_auto_observer<int>();
        flow::observable<int>{cell}.share(2).compose(subscribe_all(snk1, snk2));
        ctx->run();
        CHECK(snk1->subscribed());
        CHECK(snk2->subscribed());
        cell->set_error(sec::runtime_error);
        ctx->run();
        CHECK(snk1->aborted());
        CHECK(snk2->aborted());
      }
    }
  }
  GIVEN("an already failed connectable") {
    WHEN("subscribing to it") {
      THEN("the subscribers receive the error immediately") {
        auto cell = make_counted<flow::op::cell<int>>(ctx.get());
        auto conn = flow::observable<int>{cell}.share();
        cell->set_error(sec::runtime_error);
        // First subscriber to trigger subscription to the cell.
        auto snk1 = flow::make_auto_observer<int>();
        conn.subscribe(snk1->as_observer());
        ctx->run();
        CHECK(snk1->aborted());
        // After this point, new subscribers should be aborted right away.
        auto snk2 = flow::make_auto_observer<int>();
        auto sub = conn.subscribe(snk2->as_observer());
        CHECK(sub.disposed());
        CHECK(snk2->aborted());
        ctx->run();
      }
    }
  }
}

SCENARIO("observers that dispose their subscription do not affect others") {
  GIVEN("a connectable with two subscribers") {
    WHEN("one of the subscribers disposes its subscription") {
      THEN("the other subscriber still receives all data") {
        using impl_t = flow::op::publish<int>;
        auto snk1 = flow::make_passive_observer<int>();
        auto snk2 = flow::make_passive_observer<int>();
        auto grd = make_unsubscribe_guard(snk1, snk2);
        auto iota = ctx->make_observable().iota(1).take(12).as_observable();
        auto uut = make_counted<impl_t>(ctx.get(), iota.pimpl(), 5);
        auto sub1 = uut->subscribe(snk1->as_observer());
        auto sub2 = uut->subscribe(snk2->as_observer());
        uut->connect();
        ctx->run();
        snk1->request(7);
        snk2->request(3);
        ctx->run();
        CHECK_EQ(snk1->buf, ls(1, 2, 3, 4, 5, 6, 7));
        CHECK_EQ(snk2->buf, ls(1, 2, 3));
        snk2->sub.dispose();
        ctx->run();
        snk1->request(42);
        ctx->run();
        CHECK_EQ(snk1->buf, ls(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12));
      }
    }
  }
}

SCENARIO("publishers with auto_disconnect auto-dispose their subscription") {
  GIVEN("a connectable with two subscribers") {
    WHEN("both subscribers drop out and auto_disconnect is enabled") {
      THEN("the publisher becomes disconnected") {
        using impl_t = flow::op::publish<int>;
        auto snk1 = flow::make_passive_observer<int>();
        auto snk2 = flow::make_passive_observer<int>();
        auto grd = make_unsubscribe_guard(snk1, snk2);
        auto iota = ctx->make_observable().iota(1).take(12).as_observable();
        auto uut = make_counted<impl_t>(ctx.get(), iota.pimpl(), 5);
        auto sub1 = uut->subscribe(snk1->as_observer());
        auto sub2 = uut->subscribe(snk2->as_observer());
        uut->auto_disconnect(true);
        uut->connect();
        CHECK(uut->connected());
        ctx->run();
        snk1->request(7);
        snk2->request(3);
        ctx->run();
        CHECK_EQ(snk1->buf, ls(1, 2, 3, 4, 5, 6, 7));
        CHECK_EQ(snk2->buf, ls(1, 2, 3));
        snk1->sub.dispose();
        snk2->sub.dispose();
        ctx->run();
        CHECK(!uut->connected());
      }
    }
  }
}

SCENARIO("publishers dispose unexpected subscriptions") {
  GIVEN("an initialized publish operator") {
    WHEN("calling on_subscribe with unexpected subscriptions") {
      THEN("the operator disposes them immediately") {
        using impl_t = flow::op::publish<int>;
        auto snk = flow::make_passive_observer<int>();
        auto grd = make_unsubscribe_guard(snk);
        auto iota = ctx->make_observable().iota(1).take(12).as_observable();
        auto uut = make_counted<impl_t>(ctx.get(), iota.pimpl());
        uut->subscribe(snk->as_observer());
        uut->connect();
        auto sub = flow::make_passive_subscription();
        uut->on_subscribe(flow::subscription{sub});
        CHECK(sub->disposed());
      }
    }
  }
}

END_FIXTURE_SCOPE()
