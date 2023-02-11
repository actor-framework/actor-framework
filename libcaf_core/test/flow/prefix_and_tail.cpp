// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.prefix_and_tail

#include "caf/flow/observable.hpp"

#include "core-test.hpp"

#include <memory>

#include "caf/flow/coordinator.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/scoped_coordinator.hpp"

using namespace caf;

namespace {

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();
};

template <class T, class... Ts>
auto ls(T x, Ts... xs) {
  return std::vector<T>{x, xs...};
}

// Note: last is inclusive.
template <class T>
auto ls_range(T first, T last) {
  auto result = std::vector<T>{};
  for (; first <= last; ++first)
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
        CHECK_EQ(snk->buf, ls_range(8, 256));
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

END_FIXTURE_SCOPE()
