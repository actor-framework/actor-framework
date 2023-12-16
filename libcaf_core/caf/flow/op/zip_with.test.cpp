// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/op/zip_with.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"

#include "caf/flow/op/never.hpp"

using namespace caf;

namespace {

struct fixture : test::fixture::flow {
  template <class F, class Out, class... Ts>
  auto make_zip_with_sub(F fn, caf::flow::observer<Out> out, Ts... inputs) {
    using impl_t = caf::flow::op::zip_with_sub<F, typename Ts::output_type...>;
    auto pack = std::make_tuple(std::move(inputs).as_observable()...);
    auto sub = make_counted<impl_t>(coordinator(), std::move(fn), out, pack);
    out.on_subscribe(caf::flow::subscription{sub});
    return sub;
  }

  template <class T>
  auto make_never() {
    auto ptr = make_counted<caf::flow::op::never<T>>(coordinator());
    return caf::flow::observable<T>{std::move(ptr)};
  }

  template <class T>
  auto make_never_sub(caf::flow::observer<T> out) {
    return make_counted<caf::flow::op::never_sub<T>>(coordinator(), out);
  }
};

WITH_FIXTURE(fixture) {

SCENARIO("zip_with combines inputs") {
  GIVEN("two observables") {
    WHEN("merging them with zip_with") {
      THEN("the observer receives the combined output of both sources") {
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        make_observable()
          .zip_with([](int x, int y) { return x + y; },
                    make_observable().repeat(11).take(113),
                    make_observable().repeat(22).take(223))
          .subscribe(snk->as_observer());
        run_flows();
        require_eq(snk->state, flow::observer_state::subscribed);
        snk->sub.request(64);
        run_flows();
        check_eq(snk->state, flow::observer_state::subscribed);
        check_eq(snk->buf.size(), 64u);
        snk->sub.request(64);
        run_flows();
        check_eq(snk->state, flow::observer_state::completed);
        check_eq(snk->buf.size(), 113u);
        check_eq(snk->buf, std::vector<int>(113, 33));
      }
    }
  }
}

SCENARIO("zip_with emits nothing when zipping an empty observable") {
  GIVEN("two observables, one of them empty") {
    WHEN("merging them with zip_with") {
      THEN("the observer sees on_complete immediately") {
        using snk_t = flow::auto_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        make_observable()
          .zip_with([](int x, int y, int z) { return x + y + z; },
                    coordinator()->make_observable().repeat(11),
                    coordinator()->make_observable().repeat(22),
                    coordinator()->make_observable().empty<int>())
          .subscribe(snk->as_observer());
        run_flows();
        check(snk->buf.empty());
        check_eq(snk->state, flow::observer_state::completed);
      }
    }
  }
}

SCENARIO("zip_with aborts if an input emits an error") {
  GIVEN("two observables, one of them emits an error after some items") {
    WHEN("merging them with zip_with") {
      THEN("the observer receives all items up to the error") {
        using snk_t = flow::auto_observer<int>;
        auto obs = make_observable();
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        obs //
          .iota(1)
          .take(3)
          .concat(obs.fail<int>(sec::runtime_error))
          .zip_with([](int x, int y) { return x + y; }, obs.iota(1).take(10))
          .subscribe(snk->as_observer());
        run_flows();
        check(snk->aborted());
        check_eq(snk->buf, std::vector<int>({2, 4, 6}));
      }
    }
  }
  GIVEN("two observables, one of them emits an error immediately") {
    WHEN("merging them with zip_with") {
      THEN("the observer only receives on_error") {
        using snk_t = flow::auto_observer<int>;
        auto obs = coordinator()->make_observable();
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        obs //
          .iota(1)
          .take(3)
          .zip_with([](int x, int y) { return x + y; },
                    obs.fail<int>(sec::runtime_error))
          .subscribe(snk->as_observer());
        run_flows();
        check(snk->aborted());
        check(snk->buf.empty());
      }
    }
  }
}

SCENARIO("zip_with on an invalid observable produces an invalid observable") {
  GIVEN("a default-constructed (invalid) observable") {
    WHEN("calling zip_with on it") {
      THEN("the result is another invalid observable") {
        using snk_t = flow::auto_observer<int>;
        auto obs = coordinator()->make_observable();
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        caf::flow::observable<int>{}
          .zip_with([](int x, int y) { return x + y; }, obs.iota(1).take(10))
          .subscribe(snk->as_observer());
        run_flows();
        check(snk->aborted());
        check(snk->buf.empty());
      }
    }
  }
  GIVEN("a valid observable") {
    WHEN("calling zip_with on it with an invalid observable") {
      THEN("the result is another invalid observable") {
        using snk_t = flow::auto_observer<int>;
        auto obs = coordinator()->make_observable();
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        obs //
          .iota(1)
          .take(10)
          .zip_with([](int x, int y) { return x + y; },
                    caf::flow::observable<int>{})
          .subscribe(snk->as_observer());
        run_flows();
        check(snk->aborted());
        check(snk->buf.empty());
      }
    }
  }
}

SCENARIO("zip_with operators can be disposed at any time") {
  auto obs = coordinator()->make_observable();
  GIVEN("a zip_with operator that produces some items") {
    WHEN("calling dispose before requesting any items") {
      THEN("the observer never receives any item") {
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto sub = //
          obs      //
            .iota(1)
            .take(10)
            .zip_with([](int x, int y) { return x + y; }, obs.iota(1))
            .subscribe(snk->as_observer());
        check(!sub.disposed());
        sub.dispose();
        run_flows();
        check(snk->aborted());
      }
    }
    WHEN("calling dispose in on_subscribe") {
      THEN("the observer receives no item") {
        using snk_t = flow::canceling_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>, false);
        obs //
          .iota(1)
          .take(10)
          .zip_with([](int x, int y) { return x + y; }, obs.iota(1))
          .subscribe(snk->as_observer());
        run_flows();
        check_eq(snk->on_next_calls, 0);
      }
    }
    WHEN("calling dispose in on_next") {
      THEN("the observer receives no additional item") {
        using snk_t = flow::canceling_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>, true);
        obs //
          .iota(1)
          .take(10)
          .zip_with([](int x, int y) { return x + y; }, obs.iota(1))
          .subscribe(snk->as_observer());
        run_flows();
        check_eq(snk->on_next_calls, 1);
      }
    }
  }
}

SCENARIO("the zip_with operators disposes unexpected subscriptions") {
  GIVEN("a zip_with operator with two inputs") {
    WHEN("on_subscribe is called twice for the same input") {
      THEN("the operator disposes the subscription") {
        using caf::flow::op::zip_index;
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_zip_with_sub([](int, int) { return 0; },
                                     snk->as_observer(), make_never<int>(),
                                     make_never<int>());
        auto sub1 = make_never_sub<int>(snk->as_observer());
        auto sub2 = make_never_sub<int>(snk->as_observer());
        uut->fwd_on_subscribe(zip_index<0>{}, caf::flow::subscription{sub1});
        uut->fwd_on_subscribe(zip_index<0>{}, caf::flow::subscription{sub2});
        check(sub1->disposed());
        check(sub2->disposed());
        uut->dispose();
        run_flows();
      }
    }
  }
}

} // WITH_FIXTURE(fixture)

} // namespace
