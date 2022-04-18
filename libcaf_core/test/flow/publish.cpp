// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.publish

#include "caf/flow/observer.hpp"

#include "core-test.hpp"

#include "caf/flow/observable_builder.hpp"
#include "caf/flow/scoped_coordinator.hpp"

using namespace caf;

using caf::flow::make_observer;

namespace {

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();

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

END_FIXTURE_SCOPE()
