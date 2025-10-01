// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/flow/op/replay.hpp"

#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"

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

SCENARIO("the replay operator replays all events from the observable") {
  GIVEN("a hot observable") {
    caf::flow::multicaster<int> mcast{coordinator()};
    mcast.push(1); // Lost, no subscriber yet.
    WHEN("the observable emits values after subscribing a replay operator") {
      auto uut = make_counted<caf::flow::op::replay<int>>(coordinator());
      check(!uut->done());
      mcast.subscribe(uut->as_observer());
      mcast.push(2);
      mcast.push(3);
      mcast.push(4);
      check_eq(uut->cached_events(), 3u);
      check(!uut->done());
      mcast.close();
      check_eq(uut->cached_events(), 4u);
      check(uut->done());
      THEN("late observers receive all values from the replay operator") {
        auto completed = std::make_shared<bool>(false);
        auto values = std::make_shared<std::vector<int>>();
        caf::flow::observable<int>{uut}
          .do_on_complete([completed] { *completed = true; })
          .for_each([values](int x) { values->push_back(x); });
        run_flows();
        check_eq(*values, ls(2, 3, 4));
        check(*completed);
      }
    }
  }
}

} // WITH_FIXTURE(fixture)
