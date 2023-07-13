// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/scenario.hpp"

#include "caf/config.hpp"
#include "caf/test/caf_test_main.hpp"
#include "caf/test/nesting_error.hpp"
#include "caf/test/test.hpp"

#ifdef CAF_ENABLE_EXCEPTIONS
SCENARIO("a scenario may not contain a section") {
  auto entered_section = false;
  check_throws<caf::test::nesting_error>([this, &entered_section] {
    GIVEN("given-1") {
      WHEN("when-1") {
        SECTION("nesting error") {
          entered_section = true;
        }
      }
    }
  });
  check(!entered_section);
}
#endif

SCENARIO("each run starts with fresh local variables") {
  GIVEN("a my_int variable") {
    auto my_int = 0;
    WHEN("entering a WHEN block") {
      THEN("the local variable has its default value") {
        check_eq(my_int, 0);
        my_int = 42;
        check_eq(my_int, 42);
      }
    }
    WHEN("entering another WHEN block") {
      THEN("previous writes to the local variable are gone") {
        check_eq(my_int, 0);
      }
    }
  }
}

struct int_fixture {
  int my_int = 0;
};

WITH_FIXTURE(int_fixture) {

SCENARIO("each run starts with a fresh fixture") {
  GIVEN("a fixture with a my_int member variable") {
    WHEN("entering a WHEN block") {
      THEN("the fixture is default-constructed") {
        check_eq(my_int, 0);
        my_int = 42;
        check_eq(my_int, 42);
      }
    }
    WHEN("entering another WHEN block") {
      THEN("previous writes to the fixture are gone") {
        check_eq(my_int, 0);
      }
    }
  }
}

} // WITH_FIXTURE(int_fixture)

SCENARIO("scenario-1") {
  auto render = [this]() -> std::string {
    if (ctx_->call_stack.empty())
      return "nil";
    std::string result;
    auto i = ctx_->call_stack.begin();
    result += (*i++)->description();
    while (i != ctx_->call_stack.end()) {
      result += "/";
      result += (*i++)->description();
    }
    return result;
  };
  auto counter = 0;
  check_eq(render(), "scenario-1");
  GIVEN("given-1") {
    check_eq(++counter, 1);
    check_eq(render(), "scenario-1/given-1");
    WHEN("when-1") {
      check_eq(++counter, 2);
      check_eq(render(), "scenario-1/given-1/when-1");
      THEN("then-1") {
        check_eq(++counter, 3);
        check_eq(render(), "scenario-1/given-1/when-1/then-1");
      }
      AND_THEN("and-then-1") {
        check_eq(++counter, 4);
        check_eq(render(), "scenario-1/given-1/when-1/and-then-1");
      }
      AND_THEN("and-then-2") {
        check_eq(++counter, 5);
        check_eq(render(), "scenario-1/given-1/when-1/and-then-2");
      }
    }
    AND_WHEN("and-when-1-1") {
      check_eq(++counter, 6);
      check_eq(render(), "scenario-1/given-1/and-when-1-1");
    }
    AND_WHEN("and-when-1-2") {
      check_eq(++counter, 7);
      check_eq(render(), "scenario-1/given-1/and-when-1-2");
    }
    WHEN("when-2") {
      check_eq(++counter, 2);
      check_eq(render(), "scenario-1/given-1/when-2");
      THEN("then-1") {
        check_eq(++counter, 3);
        check_eq(render(), "scenario-1/given-1/when-2/then-1");
      }
      AND_THEN("and-then-1") {
        check_eq(++counter, 4);
        check_eq(render(), "scenario-1/given-1/when-2/and-then-1");
      }
      AND_THEN("and-then-2") {
        check_eq(++counter, 5);
        check_eq(render(), "scenario-1/given-1/when-2/and-then-2");
      }
    }
    AND_WHEN("and-when-2-1") {
      check_eq(++counter, 6);
      check_eq(render(), "scenario-1/given-1/and-when-2-1");
    }
    AND_WHEN("and-when-2-2") {
      check_eq(++counter, 7);
      check_eq(render(), "scenario-1/given-1/and-when-2-2");
    }
  }
}

CAF_TEST_MAIN()
