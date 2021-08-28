// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.action

#include "caf/detail/action.hpp"

#include "core-test.hpp"

using namespace caf;

SCENARIO("actions wrap function calls") {
  GIVEN("an action wrapping a lambda") {
    WHEN("running the action") {
      THEN("it calls the lambda and transitions from scheduled to invoked") {
        auto called = false;
        auto uut = detail::make_action([&called] { called = true; });
        CHECK(uut.scheduled());
        uut.run();
        CHECK(called);
        CHECK(uut.invoked());
      }
    }
    WHEN("disposing the action") {
      THEN("it transitions to disposed and run no longer calls the lambda") {
        auto called = false;
        auto uut = detail::make_action([&called] { called = true; });
        CHECK(uut.scheduled());
        uut.dispose();
        CHECK(uut.disposed());
        uut.run();
        CHECK(!called);
        CHECK(uut.disposed());
      }
    }
    WHEN("running the action multiple times") {
      THEN("any call after the first becomes a no-op") {
        auto n = 0;
        auto uut = detail::make_action([&n] { ++n; });
        uut.run();
        uut.run();
        uut.run();
        CHECK(uut.invoked());
        CHECK_EQ(n, 1);
      }
    }
    WHEN("re-scheduling an action after running it") {
      THEN("then the lambda gets invoked twice") {
        auto n = 0;
        auto uut = detail::make_action([&n] { ++n; });
        uut.run();
        uut.run();
        CHECK_EQ(uut.reschedule(), detail::action::state::scheduled);
        uut.run();
        uut.run();
        CHECK(uut.invoked());
        CHECK_EQ(n, 2);
      }
    }
    WHEN("converting an action to a disposable") {
      THEN("the disposable and the action point to the same impl object") {
        auto uut = detail::make_action([] {});
        auto d1 = uut.as_disposable();                 // const& overload
        auto d2 = detail::action{uut}.as_disposable(); // && overload
        CHECK_EQ(uut.ptr(), d1.ptr());
        CHECK_EQ(uut.ptr(), d2.ptr());
      }
    }
  }
}
