// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/op/combine_latest.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/flow.hpp"
#include "caf/test/scenario.hpp"

#include "caf/flow/multicaster.hpp"
#include "caf/flow/op/never.hpp"

using namespace std::literals;
using namespace caf;

namespace {

struct fixture : test::fixture::flow {
  template <class F, class Out, class... Ts>
  auto
  make_combine_latest_sub(F fn, caf::flow::observer<Out> out, Ts... inputs) {
    using impl_t
      = caf::flow::op::combine_latest_sub<F, typename Ts::output_type...>;
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

SCENARIO("combine_latest combines inputs") {
  GIVEN("two observables") {
    WHEN("merging them with combine_latest") {
      THEN("the observer receives the combined output of both sources") {
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto pub1 = caf::flow::multicaster<int>{coordinator()};
        auto pub2 = caf::flow::multicaster<int>{coordinator()};
        make_observable()
          .combine_latest([](int x, int y) { return x + y; },
                          pub1.as_observable(), pub2.as_observable())
          .subscribe(snk->as_observer());
        snk->sub.request(64);
        run_flows();
        pub1.push({1});
        pub2.push({2});
        run_flows();
        pub1.push({4});
        pub2.push({5});
        run_flows();
        pub1.close();
        pub2.push({6});
        run_flows();
        pub2.close();
        run_flows();
        check_eq(snk->state, flow::observer_state::completed);
        check_eq(snk->buf.size(), 4u);
        check_eq(snk->buf, std::vector{3, 6, 9, 10});
      }
    }
  }
}

SCENARIO("combine_latest emits nothing when combining an empty observable") {
  GIVEN("three observables, one of them empty") {
    WHEN("merging them with combine_latest") {
      THEN("the observer sees on_complete with empty buffer") {
        using snk_t = flow::auto_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        make_observable()
          .combine_latest([](int x, int y, int z) { return x + y + z; },
                          coordinator()->make_observable().repeat(11).take(64),
                          coordinator()->make_observable().repeat(22).take(64),
                          coordinator()->make_observable().empty<int>())
          .subscribe(snk->as_observer());
        snk->sub.request(64);
        run_flows();
        check(snk->buf.empty());
        check_eq(snk->state, flow::observer_state::completed);
      }
    }
  }
}

SCENARIO("combine_latest aborts if an input emits an error") {
  GIVEN("two observables, one of them emits an error after some items") {
    WHEN("merging them with combine_latest") {
      THEN("the observer receives all items up to the error") {
        using snk_t = flow::auto_observer<int>;
        auto obs = make_observable();
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        obs //
          .iota(1)
          .take(3)
          .concat(obs.fail<int>(sec::runtime_error))
          .combine_latest([](int x, int y) { return x + y; },
                          obs.iota(1).take(10))
          .subscribe(snk->as_observer());
        run_flows();
        check(snk->aborted());
        check_eq(snk->buf, std::vector<int>({11, 12, 13}));
      }
    }
  }
  GIVEN("two observables, one of them emits an error immediately") {
    WHEN("merging them with combine_latest") {
      THEN("the observer only receives on_error") {
        using snk_t = flow::auto_observer<int>;
        auto obs = coordinator()->make_observable();
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        obs //
          .iota(1)
          .take(3)
          .combine_latest([](int x, int y) { return x + y; },
                          obs.fail<int>(sec::runtime_error))
          .subscribe(snk->as_observer());
        run_flows();
        check(snk->aborted());
        check(snk->buf.empty());
      }
    }
  }
}

SCENARIO(
  "combine_latest on an invalid observable produces an invalid observable") {
  GIVEN("a default-constructed (invalid) observable") {
    WHEN("calling combine_latest on it") {
      THEN("the result is another invalid observable") {
        using snk_t = flow::auto_observer<int>;
        auto obs = coordinator()->make_observable();
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        caf::flow::observable<int>{}
          .combine_latest([](int x, int y) { return x + y; },
                          obs.iota(1).take(10))
          .subscribe(snk->as_observer());
        run_flows();
        check(snk->aborted());
        check(snk->buf.empty());
      }
    }
  }
  GIVEN("a valid observable") {
    WHEN("calling combine_latest on it with an invalid observable") {
      THEN("the result is another invalid observable") {
        using snk_t = flow::auto_observer<int>;
        auto obs = coordinator()->make_observable();
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        obs //
          .iota(1)
          .take(10)
          .combine_latest([](int x, int y) { return x + y; },
                          caf::flow::observable<int>{})
          .subscribe(snk->as_observer());
        run_flows();
        check(snk->aborted());
        check(snk->buf.empty());
      }
    }
  }
}

SCENARIO("combine_latest operators can be disposed at any time") {
  auto obs = coordinator()->make_observable();
  GIVEN("a combine_latest operator that produces some items") {
    WHEN("calling dispose before requesting any items") {
      THEN("the observer never receives any item") {
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto sub = //
          obs      //
            .iota(1)
            .take(10)
            .combine_latest([](int x, int y) { return x + y; }, obs.iota(1))
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
          .combine_latest([](int x, int y) { return x + y; }, obs.iota(1))
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
          .combine_latest([](int x, int y) { return x + y; }, obs.iota(1))
          .subscribe(snk->as_observer());
        run_flows();
        check_eq(snk->on_next_calls, 1);
      }
    }
  }
}

SCENARIO("the combine_latest operators disposes unexpected subscriptions") {
  GIVEN("a combine_latest operator with two inputs") {
    WHEN("on_subscribe is called twice for the same input") {
      THEN("the operator disposes the subscription") {
        using caf::flow::op::combine_index;
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_combine_latest_sub([](int, int) { return 0; },
                                           snk->as_observer(),
                                           make_never<int>(),
                                           make_never<int>());
        auto sub1 = make_never_sub<int>(snk->as_observer());
        auto sub2 = make_never_sub<int>(snk->as_observer());
        uut->fwd_on_subscribe(combine_index<0>{},
                              caf::flow::subscription{sub1});
        uut->fwd_on_subscribe(combine_index<0>{},
                              caf::flow::subscription{sub2});
        check(sub1->disposed());
        check(sub2->disposed());
        uut->dispose();
        run_flows();
      }
    }
  }
}

SCENARIO("the combine_latest operators replace cancelled subscriptions") {
  GIVEN("a combine_latest operator with two inputs") {
    WHEN("subscription is cancelled for an input") {
      THEN("fwd_on_subscribe replaces the subscription with current demand") {
        using caf::flow::op::combine_index;
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_combine_latest_sub([](int, int) { return 0; },
                                           snk->as_observer(),
                                           make_never<int>(),
                                           make_never<int>());
        uut->request(64);
        uut->at(combine_index<0>{}).sub.cancel();
        run_flows();
        auto sub1 = make_never_sub<int>(snk->as_observer());
        uut->fwd_on_subscribe(combine_index<0>{},
                              caf::flow::subscription{sub1});
        check(!sub1->disposed());
        uut->dispose();
        run_flows();
      }
    }
  }
}

SCENARIO("the combine_latest operators ignores callbacks after observable is "
         "completed") {
  GIVEN("a combine_latest operator with two inputs") {
    WHEN("subscription is cancelled for an input") {
      THEN("call to fwd functions are ignored") {
        using caf::flow::op::combine_index;
        using snk_t = flow::passive_observer<int>;
        auto snk = coordinator()->add_child(std::in_place_type<snk_t>);
        auto uut = make_combine_latest_sub([](int, int) { return 0; },
                                           snk->as_observer(),
                                           make_never<int>(),
                                           make_never<int>());
        uut->dispose();
        run_flows();
        auto sub1 = make_never_sub<int>(snk->as_observer());
        uut->fwd_on_subscribe(combine_index<0>{},
                              caf::flow::subscription{sub1});
        check(sub1->disposed());
        uut->fwd_on_complete(combine_index<0>{});
        uut->fwd_on_error(combine_index<0>{}, make_error(sec::disposed));
        uut->request(64);
      }
    }
  }
}

} // WITH_FIXTURE(fixture)

} // namespace
