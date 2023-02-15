// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.prefix_and_tail

#include "caf/flow/op/prefix_and_tail.hpp"

#include "core-test.hpp"

#include <memory>

#include "caf/flow/coordinator.hpp"
#include "caf/flow/observable.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/scoped_coordinator.hpp"

using namespace caf;

namespace {

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();

  // Similar to prefix_and_tail::subscribe, but returns a merge_sub pointer
  // instead of type-erasing it into a disposable.
  template <class T, class Observer>
  auto raw_sub(Observer out, size_t psize) {
    using flow::op::prefix_and_tail_sub;
    auto ptr = make_counted<prefix_and_tail_sub<T>>(ctx.get(), out, psize);
    out.on_subscribe(flow::subscription{ptr});
    return ptr;
  }
};

template <class T, class... Ts>
auto ls(T x, Ts... xs) {
  return std::vector<T>{x, xs...};
}

template <class T>
auto ls_range(T first, T last) {
  auto result = std::vector<T>{};
  for (; first < last; ++first)
    result.push_back(first);
  return result;
}

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("prefix_and_tail splits off initial elements") {
  using tuple_t = cow_tuple<cow_vector<int>, flow::observable<int>>;
  GIVEN("a generation with 0 values") {
    WHEN("calling prefix_and_tail(2)") {
      THEN("the observer of prefix_and_tail only receives on_complete") {
        auto snk = flow::make_auto_observer<tuple_t>();
        ctx->make_observable()
          .empty<int>() //
          .prefix_and_tail(2)
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK(snk->buf.empty());
        CHECK_EQ(snk->state, flow::observer_state::completed);
        CHECK_EQ(snk->err, error{});
      }
    }
  }
  GIVEN("a generation with 1 values") {
    WHEN("calling prefix_and_tail(2)") {
      THEN("the observer of prefix_and_tail only receives on_complete") {
        auto snk = flow::make_auto_observer<tuple_t>();
        ctx->make_observable()
          .just(1) //
          .prefix_and_tail(2)
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK(snk->buf.empty());
        CHECK_EQ(snk->state, flow::observer_state::completed);
        CHECK_EQ(snk->err, error{});
      }
    }
  }
  GIVEN("a generation with 2 values") {
    WHEN("calling prefix_and_tail(2)") {
      THEN("the observer receives the first 2 elements plus empty remainder") {
        auto snk = flow::make_auto_observer<int>();
        auto flat_map_calls = 0;
        ctx->make_observable()
          .iota(1)
          .take(2)
          .prefix_and_tail(2)
          .flat_map([&](const tuple_t& x) {
            ++flat_map_calls;
            auto& [prefix, tail] = x.data();
            CHECK_EQ(prefix, ls(1, 2));
            return tail;
          })
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK(snk->buf.empty());
        CHECK_EQ(flat_map_calls, 1);
        CHECK_EQ(snk->state, flow::observer_state::completed);
        CHECK_EQ(snk->err, error{});
      }
    }
  }
  GIVEN("a generation with 8 values") {
    WHEN("calling prefix_and_tail(2)") {
      THEN("the observer receives the first 2 elements plus remainder") {
        auto snk = flow::make_auto_observer<int>();
        auto flat_map_calls = 0;
        ctx->make_observable()
          .iota(1)
          .take(8)
          .prefix_and_tail(2)
          .flat_map([&](const tuple_t& x) {
            ++flat_map_calls;
            auto& [prefix, tail] = x.data();
            CHECK_EQ(prefix, ls(1, 2));
            return tail;
          })
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK_EQ(flat_map_calls, 1);
        CHECK_EQ(snk->buf, ls(3, 4, 5, 6, 7, 8));
        CHECK_EQ(snk->state, flow::observer_state::completed);
        CHECK_EQ(snk->err, error{});
      }
    }
  }
  GIVEN("a generation with 256 values") {
    WHEN("calling prefix_and_tail(7)") {
      THEN("the observer receives the first 7 elements plus remainder") {
        auto snk = flow::make_auto_observer<int>();
        auto flat_map_calls = 0;
        ctx->make_observable()
          .iota(1)
          .take(256)
          .prefix_and_tail(7)
          .flat_map([&](const tuple_t& x) {
            ++flat_map_calls;
            auto& [prefix, tail] = x.data();
            CHECK_EQ(prefix, ls(1, 2, 3, 4, 5, 6, 7));
            return tail;
          })
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK_EQ(flat_map_calls, 1);
        CHECK_EQ(snk->buf, ls_range(8, 257));
        CHECK_EQ(snk->state, flow::observer_state::completed);
        CHECK_EQ(snk->err, error{});
      }
    }
  }
}

SCENARIO("head_and_tail splits off the first element") {
  using tuple_t = cow_tuple<int, flow::observable<int>>;
  GIVEN("a generation with 0 values") {
    WHEN("calling head_and_tail") {
      THEN("the observer of head_and_tail only receives on_complete") {
        auto snk = flow::make_auto_observer<tuple_t>();
        ctx->make_observable()
          .empty<int>() //
          .head_and_tail()
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK(snk->buf.empty());
        CHECK_EQ(snk->state, flow::observer_state::completed);
        CHECK_EQ(snk->err, error{});
      }
    }
  }
  GIVEN("a generation with 1 values") {
    WHEN("calling head_and_tail()") {
      THEN("the observer receives the first element plus empty remainder") {
        auto snk = flow::make_auto_observer<int>();
        auto flat_map_calls = 0;
        ctx->make_observable()
          .just(1)
          .head_and_tail()
          .flat_map([&](const tuple_t& x) {
            ++flat_map_calls;
            auto& [prefix, tail] = x.data();
            CHECK_EQ(prefix, 1);
            return tail;
          })
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK(snk->buf.empty());
        CHECK_EQ(flat_map_calls, 1);
        CHECK_EQ(snk->state, flow::observer_state::completed);
        CHECK_EQ(snk->err, error{});
      }
    }
  }
  GIVEN("a generation with 2 values") {
    WHEN("calling head_and_tail()") {
      THEN("the observer receives the first element plus remainder") {
        auto snk = flow::make_auto_observer<int>();
        auto flat_map_calls = 0;
        ctx->make_observable()
          .iota(1)
          .take(2)
          .head_and_tail()
          .flat_map([&](const tuple_t& x) {
            ++flat_map_calls;
            auto& [prefix, tail] = x.data();
            CHECK_EQ(prefix, 1);
            return tail;
          })
          .subscribe(snk->as_observer());
        ctx->run();
        CHECK_EQ(flat_map_calls, 1);
        CHECK_EQ(snk->buf, ls(2));
        CHECK_EQ(snk->state, flow::observer_state::completed);
        CHECK_EQ(snk->err, error{});
      }
    }
  }
}

SCENARIO("head_and_tail forwards errors") {
  using tuple_t = cow_tuple<int, flow::observable<int>>;
  GIVEN("an observable that emits on_error only") {
    WHEN("applying a head_and_tail operator to it") {
      THEN("the observer for the head receives on_error") {
        auto failed = false;
        auto got_tail = false;
        ctx->make_observable()
          .fail<int>(sec::runtime_error)
          .head_and_tail()
          .do_on_error([&failed](const error& what) {
            failed = true;
            CHECK_EQ(what, sec::runtime_error);
          })
          .for_each([&got_tail](const tuple_t&) { got_tail = true; });
        ctx->run();
        CHECK(failed);
        CHECK(!got_tail);
      }
    }
  }
  GIVEN("an observable that emits one value and then on_error") {
    WHEN("applying a head_and_tail operator to it") {
      THEN("the observer for the tail receives on_error") {
        auto head_failed = false;
        auto tail_failed = false;
        auto got_tail = false;
        auto tail_values = 0;
        ctx->make_observable()
          .just(1)
          .concat(ctx->make_observable().fail<int>(sec::runtime_error))
          .head_and_tail()
          .do_on_error([&head_failed](const error&) { head_failed = true; })
          .flat_map([&](const tuple_t& x) {
            auto& [head, tail] = x.data();
            got_tail = true;
            CHECK_EQ(head, 1);
            return tail;
          })
          .do_on_error([&tail_failed](const error& what) {
            tail_failed = true;
            CHECK_EQ(what, sec::runtime_error);
          })
          .for_each([&tail_values](int) { ++tail_values; });
        ctx->run();
        CHECK(got_tail);
        CHECK(!head_failed);
        CHECK(tail_failed);
        CHECK_EQ(tail_values, 0);
      }
    }
  }
}

SCENARIO("head_and_tail requests the prefix as soon as possible") {
  using tuple_t = cow_tuple<cow_vector<int>, flow::observable<int>>;
  GIVEN("an observable that delays the call to on_subscribe") {
    WHEN("the observer requests before on_subscribe from the input arrives") {
      THEN("head_and_tail requests the prefix immediately") {
        auto snk = flow::make_passive_observer<tuple_t>();
        auto uut = raw_sub<int>(snk->as_observer(), 7);
        snk->request(42);
        ctx->run();
        auto in_sub = flow::make_passive_subscription();
        uut->on_subscribe(flow::subscription{in_sub});
        CHECK_EQ(in_sub->demand, 7u);
      }
    }
  }
}

SCENARIO("head_and_tail disposes unexpected subscriptions") {
  using tuple_t = cow_tuple<cow_vector<int>, flow::observable<int>>;
  GIVEN("a subscribed head_and_tail operator") {
    WHEN("on_subscribe gets called again") {
      THEN("the unexpected subscription gets disposed") {
        auto snk = flow::make_passive_observer<tuple_t>();
        auto uut = raw_sub<int>(snk->as_observer(), 7);
        auto sub1 = flow::make_passive_subscription();
        auto sub2 = flow::make_passive_subscription();
        uut->on_subscribe(flow::subscription{sub1});
        uut->on_subscribe(flow::subscription{sub2});
        CHECK(!sub1->disposed());
        CHECK(sub2->disposed());
      }
    }
  }
}

SCENARIO("disposing head_and_tail disposes the input subscription") {
  using tuple_t = cow_tuple<cow_vector<int>, flow::observable<int>>;
  GIVEN("a subscribed head_and_tail operator") {
    WHEN("calling dispose on the operator") {
      THEN("the operator disposes its input") {
        auto snk = flow::make_passive_observer<tuple_t>();
        auto uut = raw_sub<int>(snk->as_observer(), 7);
        auto sub = flow::make_passive_subscription();
        uut->on_subscribe(flow::subscription{sub});
        CHECK(!uut->disposed());
        CHECK(!sub->disposed());
        uut->dispose();
        CHECK(uut->disposed());
        CHECK(sub->disposed());
      }
    }
  }
}

SCENARIO("disposing the tail of head_and_tail disposes the operator") {
  using tuple_t = cow_tuple<cow_vector<int>, flow::observable<int>>;
  GIVEN("a subscribed head_and_tail operator") {
    WHEN("calling dispose the subscription to the tail") {
      THEN("the operator gets disposed") {
        auto got_tail = false;
        auto tail_values = 0;
        auto snk = flow::make_passive_observer<int>();
        auto sub = //
          ctx->make_observable()
            .iota(1) //
            .take(7)
            .prefix_and_tail(3)
            .for_each([&](const tuple_t& x) {
              got_tail = true;
              auto [prefix, tail] = x.data();
              auto sub = tail.subscribe(snk->as_observer());
              sub.dispose();
            });
        ctx->run();
        CHECK(got_tail);
        CHECK_EQ(tail_values, 0);
        CHECK(sub.disposed());
        CHECK(snk->completed());
      }
    }
  }
}

END_FIXTURE_SCOPE()
