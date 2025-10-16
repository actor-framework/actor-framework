// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/flow/op/cache.hpp"

#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/flow/multicaster.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/make_counted.hpp"

using namespace caf;

namespace {

struct fixture : test::fixture::flow {
  // Similar to prefix_and_tail::subscribe, but returns a merge_sub pointer
  // instead of type-erasing it into a disposable.
  template <class T, class Observer>
  auto raw_sub(Observer out, size_t psize) {
    using caf::flow::op::prefix_and_tail_sub;
    auto ptr = make_counted<prefix_and_tail_sub<T>>(coordinator(), out, psize);
    out.on_subscribe(caf::flow::subscription{ptr});
    return ptr;
  }

  template <class T>
  auto make_never_sub(caf::flow::observer<T> out) {
    return make_counted<caf::flow::op::never_sub<T>>(coordinator(), out);
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

WITH_FIXTURE(fixture) {

SCENARIO("the cache operator caches all events from its input observable") {
  using cache_t = caf::flow::op::cache<int>;
  GIVEN("a hot observable") {
    caf::flow::multicaster<int> mcast{coordinator()};
    mcast.push(1); // Lost, no subscriber yet.
    WHEN("the cache operator is subscribed to while messages are generated") {
      THEN("subscribers receive values from the cache operator immediately") {
        auto uut = make_counted<cache_t>(coordinator(), mcast.pimpl());
        auto snk = make_auto_observer<int>();
        uut->subscribe(snk->as_observer());
        run_flows();
        mcast.push(2);
        mcast.push(3);
        mcast.push(4);
        check_eq(snk->buf, ls(2, 3, 4)); // must be forwarded without delay
        check(snk->subscribed());
        mcast.close();
        check(snk->completed());
      }
    }
    WHEN("the cache operator is subscribed to after messages are generated") {
      THEN("late observers receive cached values from the cache operator") {
        auto uut = make_counted<cache_t>(coordinator(), mcast.pimpl());
        uut->subscribe_to_source(); // Subscribe to the source eagerly.
        mcast.push(2);
        mcast.push(3);
        mcast.push(4);
        check_eq(uut->cached_events(), 3u);
        check(!uut->done());
        mcast.close();
        check_eq(uut->cached_events(), 4u);
        check(uut->done());
        auto snk = make_auto_observer<int>();
        uut->subscribe(snk->as_observer());
        run_flows();
        check_eq(snk->buf, ls(2, 3, 4));
        check(snk->completed());
      }
    }
    WHEN("some observers subscribe to the cache operator late") {
      THEN("they receive the cached values and live data afterwards") {
        auto uut = make_counted<cache_t>(coordinator(), mcast.pimpl());
        auto snk1 = make_auto_observer<int>();
        uut->subscribe(snk1->as_observer());
        run_flows();
        mcast.push(2);
        mcast.push(3);
        check_eq(snk1->buf, ls(2, 3));
        auto snk2 = make_auto_observer<int>();
        uut->subscribe(snk2->as_observer());
        run_flows();
        check_eq(snk1->buf, ls(2, 3));
        check_eq(snk2->buf, ls(2, 3));
        check(snk1->subscribed());
        check(snk2->subscribed());
        mcast.push(4);
        mcast.push(5);
        mcast.close();
        check_eq(snk1->buf, ls(2, 3, 4, 5));
        check_eq(snk2->buf, ls(2, 3, 4, 5));
        check(snk1->completed());
        check(snk2->completed());
      }
    }
    WHEN("a subscriber has its subscription disposed") {
      THEN("other subscribers still receive the cached values") {
        auto uut = make_counted<cache_t>(coordinator(), mcast.pimpl());
        auto snk1 = make_auto_observer<int>();
        auto sub = uut->subscribe(snk1->as_observer());
        run_flows();
        mcast.push(2);
        mcast.push(3);
        check_eq(snk1->buf, ls(2, 3));
        auto snk2 = make_auto_observer<int>();
        uut->subscribe(snk2->as_observer());
        run_flows();
        check_eq(snk1->buf, ls(2, 3));
        check_eq(snk2->buf, ls(2, 3));
        check(snk1->subscribed());
        check(snk2->subscribed());
        mcast.push(4);
        sub.dispose(); // Dispose first observer.
        run_flows();
        mcast.push(5); // Second observer must still receive the live data.
        mcast.close();
        check_eq(snk1->buf, ls(2, 3, 4));
        check_eq(snk2->buf, ls(2, 3, 4, 5));
        if (check(snk1->aborted())) {
          check_eq(snk1->err, sec::disposed);
        }
        check(snk2->completed());
      }
    }
    WHEN("a subscriber cancels its subscription") {
      THEN("other subscribers still receive the cached values") {
        auto uut = make_counted<cache_t>(coordinator(), mcast.pimpl());
        auto snk1 = make_auto_observer<int>();
        auto sub = uut->subscribe(snk1->as_observer());
        run_flows();
        mcast.push(2);
        mcast.push(3);
        check_eq(snk1->buf, ls(2, 3));
        auto snk2 = make_canceling_observer<int>(true); // cancel in on_next
        uut->subscribe(snk2->as_observer());
        run_flows();
        check_eq(snk1->buf, ls(2, 3));
        check(snk1->subscribed());
        mcast.push(4);
        auto snk3 = make_canceling_observer<int>(); // cancel immediately
        uut->subscribe(snk3->as_observer());
        mcast.push(5);
        mcast.close();
        check_eq(snk1->buf, ls(2, 3, 4, 5));
        check(snk1->completed());
      }
    }
  }
  GIVEN("an observable that fails") {
    auto input = make_observable().fail<int>(sec::runtime_error);
    WHEN("the cache operator is subscribed to") {
      THEN("the observer receives an on_error event") {
        auto uut = make_counted<cache_t>(coordinator(), input.pimpl());
        auto snk = make_auto_observer<int>();
        uut->subscribe(snk->as_observer());
        run_flows();
        check(snk->aborted());
      }
    }
  }
}

TEST("high-level API") {
  auto values = std::make_shared<std::vector<int>>();
  auto add_value = [values](int x) { values->push_back(x); };
  auto input = [this] { return make_observable().range(1, 10); };
  SECTION("observable<T>::cache") {
    input().as_observable().cache().for_each(add_value);
  }
  SECTION("observable_def<T>::cache") {
    input().cache().for_each(add_value);
  }
  run_flows();
  check_eq(*values, ls(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
}

} // WITH_FIXTURE(fixture)
