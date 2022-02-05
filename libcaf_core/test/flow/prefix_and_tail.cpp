// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.prefix_and_tail

#include "caf/flow/observable.hpp"

#include "core-test.hpp"

#include <memory>

#include "caf/flow/coordinator.hpp"
#include "caf/flow/merge.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/scoped_coordinator.hpp"

using namespace caf;

namespace {

template <class T>
struct test_observer {
  void on_next(T x) {
    values.emplace_back(std::move(x));
  }

  void on_error(const error& what) {
    had_error = true;
    err = what;
  }

  void on_complete() {
    had_complete = true;
  }

  std::vector<T> values;
  bool had_error = false;
  bool had_complete = false;
  error err;
};

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("prefix_and_tail splits off initial elements") {
  using tuple_t = cow_tuple<std::vector<int>, flow::observable<int>>;
  GIVEN("a generation with 0 values") {
    WHEN("calling prefix_and_tail(2)") {
      THEN("the observer of prefix_and_tail only receives on_complete") {
        auto inputs = std::vector<int>{};
        auto obs = std::make_shared<test_observer<tuple_t>>();
        ctx->make_observable()
          .from_container(inputs)
          .prefix_and_tail(2)
          .subscribe(flow::make_observer_from_ptr(obs));
        ctx->run();
        CHECK(obs->had_complete);
        CHECK(!obs->had_error);
        CHECK(obs->values.empty());
      }
    }
  }
  GIVEN("a generation with 1 values") {
    WHEN("calling prefix_and_tail(2)") {
      THEN("the observer of prefix_and_tail only receives on_complete") {
        auto inputs = std::vector<int>{1};
        auto obs = std::make_shared<test_observer<tuple_t>>();
        ctx->make_observable()
          .from_container(inputs)
          .prefix_and_tail(2)
          .subscribe(flow::make_observer_from_ptr(obs));
        ctx->run();
        CHECK(obs->had_complete);
        CHECK(!obs->had_error);
        CHECK(obs->values.empty());
      }
    }
  }
  GIVEN("a generation with 2 values") {
    WHEN("calling prefix_and_tail(2)") {
      THEN("the observer receives the first 2 elements plus empty remainder") {
        auto inputs = std::vector<int>{1, 2};
        auto prefix_vals = std::vector<int>{1, 2};
        auto tail_vals = std::vector<int>{};
        auto obs = std::make_shared<test_observer<int>>();
        auto flat_map_calls = 0;
        ctx->make_observable()
          .from_container(inputs)
          .prefix_and_tail(2)
          .flat_map([&](const tuple_t& x) {
            ++flat_map_calls;
            auto& [prefix, tail] = x.data();
            CHECK_EQ(prefix, prefix_vals);
            return tail;
          })
          .subscribe(flow::make_observer_from_ptr(obs));
        ctx->run();
        CHECK_EQ(flat_map_calls, 1);
        CHECK_EQ(obs->values, tail_vals);
        CHECK(obs->had_complete);
        CHECK(!obs->had_error);
      }
    }
  }
  GIVEN("a generation with 8 values") {
    WHEN("calling prefix_and_tail(2)") {
      THEN("the observer receives the first 2 elements plus remainder") {
        auto inputs = std::vector<int>{1, 2, 4, 8, 16, 32, 64, 128};
        auto prefix_vals = std::vector<int>{1, 2};
        auto tail_vals = std::vector<int>{4, 8, 16, 32, 64, 128};
        auto obs = std::make_shared<test_observer<int>>();
        auto flat_map_calls = 0;
        ctx->make_observable()
          .from_container(inputs)
          .prefix_and_tail(2)
          .flat_map([&](const tuple_t& x) {
            ++flat_map_calls;
            auto& [prefix, tail] = x.data();
            CHECK_EQ(prefix, prefix_vals);
            return tail;
          })
          .subscribe(flow::make_observer_from_ptr(obs));
        ctx->run();
        CHECK_EQ(flat_map_calls, 1);
        CHECK_EQ(obs->values, tail_vals);
        CHECK(obs->had_complete);
        CHECK(!obs->had_error);
      }
    }
  }
}

SCENARIO("head_and_tail splits off the first element") {
  using tuple_t = cow_tuple<int, flow::observable<int>>;
  GIVEN("a generation with 0 values") {
    WHEN("calling head_and_tail") {
      THEN("the observer of head_and_tail only receives on_complete") {
        auto inputs = std::vector<int>{};
        auto obs = std::make_shared<test_observer<tuple_t>>();
        ctx->make_observable()
          .from_container(inputs)
          .head_and_tail()
          .subscribe(flow::make_observer_from_ptr(obs));
        ctx->run();
        CHECK(obs->had_complete);
        CHECK(!obs->had_error);
        CHECK(obs->values.empty());
      }
    }
  }
  GIVEN("a generation with 1 values") {
    WHEN("calling head_and_tail()") {
      THEN("the observer receives the first element plus empty remainder") {
        auto inputs = std::vector<int>{1};
        auto prefix_val = 1;
        auto tail_vals = std::vector<int>{};
        auto obs = std::make_shared<test_observer<int>>();
        auto flat_map_calls = 0;
        ctx->make_observable()
          .from_container(inputs)
          .head_and_tail()
          .flat_map([&](const tuple_t& x) {
            ++flat_map_calls;
            auto& [prefix, tail] = x.data();
            CHECK_EQ(prefix, prefix_val);
            return tail;
          })
          .subscribe(flow::make_observer_from_ptr(obs));
        ctx->run();
        CHECK_EQ(flat_map_calls, 1);
        CHECK_EQ(obs->values, tail_vals);
        CHECK(obs->had_complete);
        CHECK(!obs->had_error);
      }
    }
  }
  GIVEN("a generation with 2 values") {
    WHEN("calling head_and_tail()") {
      THEN("the observer receives the first element plus remainder") {
        auto inputs = std::vector<int>{1, 2};
        auto prefix_val = 1;
        auto tail_vals = std::vector<int>{2};
        auto obs = std::make_shared<test_observer<int>>();
        auto flat_map_calls = 0;
        ctx->make_observable()
          .from_container(inputs)
          .head_and_tail()
          .flat_map([&](const tuple_t& x) {
            ++flat_map_calls;
            auto& [prefix, tail] = x.data();
            CHECK_EQ(prefix, prefix_val);
            return tail;
          })
          .subscribe(flow::make_observer_from_ptr(obs));
        ctx->run();
        CHECK_EQ(flat_map_calls, 1);
        CHECK_EQ(obs->values, tail_vals);
        CHECK(obs->had_complete);
        CHECK(!obs->had_error);
      }
    }
  }
}

END_FIXTURE_SCOPE()
