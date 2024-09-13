// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/flow/op/prefix_and_tail.hpp"

#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/flow/op/never.hpp"

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

SCENARIO("prefix_and_tail splits off initial elements") {
  using tuple_t = cow_tuple<cow_vector<int>, caf::flow::observable<int>>;
  GIVEN("a generation with 0 values") {
    WHEN("calling prefix_and_tail(2)") {
      THEN("the observer of prefix_and_tail only receives on_complete") {
        using snk_t = flow::auto_observer<tuple_t>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        coordinator()
          ->make_observable()
          .empty<int>() //
          .prefix_and_tail(2)
          .subscribe(snk->as_observer());
        run_flows();
        check(snk->buf.empty());
        check_eq(snk->state, flow::observer_state::completed);
        check_eq(snk->err, error{});
      }
    }
  }
  GIVEN("a generation with 1 values") {
    WHEN("calling prefix_and_tail(2)") {
      THEN("the observer of prefix_and_tail only receives on_complete") {
        using snk_t = flow::auto_observer<tuple_t>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        coordinator()
          ->make_observable()
          .just(1) //
          .prefix_and_tail(2)
          .subscribe(snk->as_observer());
        run_flows();
        check(snk->buf.empty());
        check_eq(snk->state, flow::observer_state::completed);
        check_eq(snk->err, error{});
      }
    }
  }
  GIVEN("a generation with 2 values") {
    WHEN("calling prefix_and_tail(2)") {
      THEN("the observer receives the first 2 elements plus empty remainder") {
        using snk_t = flow::auto_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto flat_map_calls = 0;
        coordinator()
          ->make_observable()
          .iota(1)
          .take(2)
          .prefix_and_tail(2)
          .flat_map([&](const tuple_t& x) {
            ++flat_map_calls;
            auto& [prefix, tail] = x.data();
            check_eq(prefix, ls(1, 2));
            return tail;
          })
          .subscribe(snk->as_observer());
        run_flows();
        check(snk->buf.empty());
        check_eq(flat_map_calls, 1);
        check_eq(snk->state, flow::observer_state::completed);
        check_eq(snk->err, error{});
      }
    }
  }
  GIVEN("a generation with 8 values") {
    WHEN("calling prefix_and_tail(2)") {
      THEN("the observer receives the first 2 elements plus remainder") {
        using snk_t = flow::auto_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto flat_map_calls = 0;
        coordinator()
          ->make_observable()
          .iota(1)
          .take(8)
          .prefix_and_tail(2)
          .flat_map([&](const tuple_t& x) {
            ++flat_map_calls;
            auto& [prefix, tail] = x.data();
            check_eq(prefix, ls(1, 2));
            return tail;
          })
          .subscribe(snk->as_observer());
        run_flows();
        check_eq(flat_map_calls, 1);
        check_eq(snk->buf, ls(3, 4, 5, 6, 7, 8));
        check_eq(snk->state, flow::observer_state::completed);
        check_eq(snk->err, error{});
      }
    }
  }
  GIVEN("a generation with 256 values") {
    WHEN("calling prefix_and_tail(7)") {
      THEN("the observer receives the first 7 elements plus remainder") {
        using snk_t = flow::auto_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto flat_map_calls = 0;
        coordinator()
          ->make_observable()
          .iota(1)
          .take(256)
          .prefix_and_tail(7)
          .flat_map([&](const tuple_t& x) {
            ++flat_map_calls;
            auto& [prefix, tail] = x.data();
            check_eq(prefix, ls(1, 2, 3, 4, 5, 6, 7));
            return tail;
          })
          .subscribe(snk->as_observer());
        run_flows();
        check_eq(flat_map_calls, 1);
        check_eq(snk->buf, ls_range(8, 257));
        check_eq(snk->state, flow::observer_state::completed);
        check_eq(snk->err, error{});
      }
    }
  }
}

SCENARIO("head_and_tail splits off the first element") {
  using tuple_t = cow_tuple<int, caf::flow::observable<int>>;
  GIVEN("a generation with 0 values") {
    WHEN("calling head_and_tail") {
      THEN("the observer of head_and_tail only receives on_complete") {
        using snk_t = flow::auto_observer<tuple_t>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        coordinator()
          ->make_observable()
          .empty<int>() //
          .head_and_tail()
          .subscribe(snk->as_observer());
        run_flows();
        check(snk->buf.empty());
        check_eq(snk->state, flow::observer_state::completed);
        check_eq(snk->err, error{});
      }
    }
  }
  GIVEN("a generation with 1 values") {
    WHEN("calling head_and_tail()") {
      THEN("the observer receives the first element plus empty remainder") {
        using snk_t = flow::auto_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto flat_map_calls = 0;
        coordinator()
          ->make_observable()
          .just(1)
          .head_and_tail()
          .flat_map([&](const tuple_t& x) {
            ++flat_map_calls;
            auto& [prefix, tail] = x.data();
            check_eq(prefix, 1);
            return tail;
          })
          .subscribe(snk->as_observer());
        run_flows();
        check(snk->buf.empty());
        check_eq(flat_map_calls, 1);
        check_eq(snk->state, flow::observer_state::completed);
        check_eq(snk->err, error{});
      }
    }
  }
  GIVEN("a generation with 2 values") {
    WHEN("calling head_and_tail()") {
      THEN("the observer receives the first element plus remainder") {
        using snk_t = flow::auto_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto flat_map_calls = 0;
        coordinator()
          ->make_observable()
          .iota(1)
          .take(2)
          .head_and_tail()
          .flat_map([&](const tuple_t& x) {
            ++flat_map_calls;
            auto& [prefix, tail] = x.data();
            check_eq(prefix, 1);
            return tail;
          })
          .subscribe(snk->as_observer());
        run_flows();
        check_eq(flat_map_calls, 1);
        check_eq(snk->buf, ls(2));
        check_eq(snk->state, flow::observer_state::completed);
        check_eq(snk->err, error{});
      }
    }
  }
}

SCENARIO("head_and_tail forwards errors") {
  using tuple_t = cow_tuple<int, caf::flow::observable<int>>;
  GIVEN("an observable that emits on_error only") {
    WHEN("applying a head_and_tail operator to it") {
      THEN("the observer for the head receives on_error") {
        auto failed = false;
        auto got_tail = false;
        coordinator()
          ->make_observable()
          .fail<int>(sec::runtime_error)
          .head_and_tail()
          .do_on_error([&failed, this](const error& what) {
            failed = true;
            check_eq(what, sec::runtime_error);
          })
          .for_each([&got_tail](const tuple_t&) { got_tail = true; });
        run_flows();
        check(failed);
        check(!got_tail);
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
        coordinator()
          ->make_observable()
          .just(1)
          .concat(
            coordinator()->make_observable().fail<int>(sec::runtime_error))
          .head_and_tail()
          .do_on_error([&head_failed](const error&) { head_failed = true; })
          .flat_map([&](const tuple_t& x) {
            auto& [head, tail] = x.data();
            got_tail = true;
            check_eq(head, 1);
            return tail;
          })
          .do_on_error([&tail_failed, this](const error& what) {
            tail_failed = true;
            check_eq(what, sec::runtime_error);
          })
          .for_each([&tail_values](int) { ++tail_values; });
        run_flows();
        check(got_tail);
        check(!head_failed);
        check(tail_failed);
        check_eq(tail_values, 0);
      }
    }
  }
}

SCENARIO("head_and_tail disposes unexpected subscriptions") {
  using tuple_t = cow_tuple<cow_vector<int>, caf::flow::observable<int>>;
  GIVEN("a subscribed head_and_tail operator") {
    WHEN("on_subscribe gets called again") {
      THEN("the unexpected subscription gets disposed") {
        using snk_t = flow::passive_observer<tuple_t>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = raw_sub<int>(snk->as_observer(), 7);
        auto sub1 = make_never_sub<tuple_t>(snk->as_observer());
        auto sub2 = make_never_sub<tuple_t>(snk->as_observer());
        uut->on_subscribe(caf::flow::subscription{sub1});
        uut->on_subscribe(caf::flow::subscription{sub2});
        check(!sub1->disposed());
        check(sub2->disposed());
        snk->unsubscribe();
      }
    }
  }
}

SCENARIO("disposing head_and_tail disposes the input subscription") {
  using tuple_t = cow_tuple<cow_vector<int>, caf::flow::observable<int>>;
  GIVEN("a subscribed head_and_tail operator") {
    WHEN("calling dispose on the operator") {
      THEN("the operator disposes its input") {
        using snk_t = flow::passive_observer<tuple_t>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = raw_sub<int>(snk->as_observer(), 7);
        auto sub = make_never_sub<tuple_t>(snk->as_observer());
        uut->on_subscribe(caf::flow::subscription{sub});
        check(!uut->disposed());
        check(!sub->disposed());
        uut->dispose();
        run_flows();
        check(uut->disposed());
        check(sub->disposed());
      }
    }
  }
}

SCENARIO("disposing the tail of head_and_tail disposes the operator") {
  using tuple_t = cow_tuple<cow_vector<int>, caf::flow::observable<int>>;
  GIVEN("a subscribed head_and_tail operator") {
    WHEN("calling dispose the subscription to the tail") {
      THEN("the operator gets disposed") {
        using snk_t = flow::passive_observer<int>;
        auto got_tail = false;
        auto tail_values = 0;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto sub = //
          coordinator()
            ->make_observable()
            .iota(1) //
            .take(7)
            .prefix_and_tail(3)
            .for_each([&](const tuple_t& x) {
              got_tail = true;
              auto [prefix, tail] = x.data();
              auto sub = tail.subscribe(snk->as_observer());
              sub.dispose();
            });
        run_flows();
        check(got_tail);
        check_eq(tail_values, 0);
        check(sub.disposed());
        check(snk->aborted());
      }
      run_flows();
    }
  }
}

} // WITH_FIXTURE(fixture)
