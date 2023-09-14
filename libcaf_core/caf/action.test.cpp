// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/action.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"

#include "caf/event_based_actor.hpp"

using namespace caf;

SCENARIO("actions wrap function calls") {
  GIVEN("an action wrapping a lambda") {
    WHEN("running the action") {
      auto called = false;
      auto uut = make_action([&called] { called = true; });
      THEN("it calls the lambda and transitions from scheduled to invoked") {
        check(uut.scheduled());
        uut.run();
        check(called);
      }
      AND_THEN("the action is returned to the scheduled state") {
        check_eq(uut.ptr()->current_state(), action::state::scheduled);
      }
    }
    WHEN("disposing the action") {
      THEN("it transitions to disposed and run no longer calls the lambda") {
        auto called = false;
        auto uut = make_action([&called] { called = true; });
        check(uut.scheduled());
        uut.dispose();
        check(uut.disposed());
        uut.run();
        check(!called);
        check(uut.disposed());
      }
    }
    WHEN("disposing an running action") {
      THEN("it transitions to deferred_dispose and finally to disposed") {
        auto called = false;
        action uut;
        uut = make_action([this, &called, &uut] {
          called = true;
          uut.dispose();
          check_eq(uut.ptr()->current_state(), action::state::deferred_dispose);
        });
        check(uut.scheduled());
        uut.run();
        check(called);
        check_eq(uut.ptr()->current_state(), action::state::disposed);
      }
    }
    WHEN("running the action multiple times") {
      THEN("the action invokes its function that many times") {
        auto n = 0;
        auto uut = make_action([&n] { ++n; });
        uut.run();
        uut.run();
        uut.run();
        check_eq(n, 3);
      }
    }
    WHEN("converting an action to a disposable") {
      THEN("the disposable and the action point to the same impl object") {
        auto uut = make_action([] {});
        auto d1 = uut.as_disposable();         // const& overload
        auto d2 = action{uut}.as_disposable(); // && overload
        check_eq(uut.ptr(), d1.ptr());
        check_eq(uut.ptr(), d2.ptr());
      }
    }
  }
  GIVEN("a single shot action") {
    WHEN("running the action") {
      auto called = false;
      auto uut = make_single_shot_action([&called] { called = true; });
      THEN("it transitions to disposed on the first run") {
        check(uut.scheduled());
        uut.run();
        check(called);
        check(uut.disposed());
      }
      AND_THEN("running the action again has no effect") {
        called = false;
        uut.run();
        check(!called);
        check(uut.disposed());
      }
    }
    WHEN("disposing an running single shot action") {
      THEN("dispose() has no effect since it is already disposed") {
        auto called = false;
        action uut;
        uut = make_single_shot_action([this, &called, &uut] {
          called = true;
          uut.dispose();
          // No deferred dispose for single shot actions.
          check_eq(uut.ptr()->current_state(), action::state::disposed);
        });
        check(uut.scheduled());
        uut.run();
        check(called);
        check_eq(uut.ptr()->current_state(), action::state::disposed);
      }
    }
  }
}

WITH_FIXTURE(test::fixture::deterministic) {

SCENARIO("actors run actions that they receive") {
  GIVEN("a scheduled actor") {
    WHEN("sending it an action") {
      THEN("the actor runs the action regardless of its behavior") {
        auto aut = sys.spawn([](caf::event_based_actor*) -> behavior {
          return {
            [](int32_t x) { return x; },
          };
        });
        auto n = 0;
        inject().with(make_action([&n] { ++n; })).to(aut);
        check_eq(n, 1);
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

CAF_TEST_MAIN()
