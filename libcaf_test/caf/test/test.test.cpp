// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/test.hpp"

#include "caf/test/caf_test_main.hpp"

using caf::test::block_type;

TEST("tests can contain different type of checks") {
  auto* rep = caf::test::reporter::instance;
  /// tests all of the possible scenario for check_ne
  check_ne(0u, 0u);
  check_ne(0u, 1u);
  auto stats = rep->test_stats();
  /// tests all of the possible scenario for check_eq
  check_eq(stats.passed, 1u);
  check_eq(stats.passed, 0u);
  /// tests all of the possible scenario for check_ge
  check_ge(stats.passed, 0u);
  check_ge(stats.passed, 1u);
  check_ge(stats.passed, 2u);
  /// tests all of the possible scenario for check_gt
  check_gt(stats.passed, 0u);
  check_gt(stats.passed, 1u);
  check_gt(stats.passed, 2u);
  /// tests all of the possible scenario for check_le
  check_le(stats.passed, 0u);
  check_le(stats.passed, 1u);
  check_le(stats.passed, 2u);
  /// tests all of the possible scenario for check_lt
  check_lt(stats.passed, 0u);
  check_lt(stats.passed, 1u);
  check_lt(stats.passed, 2u);
  info("reset error count to not fail the test");
  caf::test::reporter::instance->test_stats({16, 0});
  info("this test had {} checks", rep->test_stats().total());
}

TEST("failed checks increment the failed counter") {
  check_eq(1, 2);
  auto stats = caf::test::reporter::instance->test_stats();
  check_eq(stats.passed, 0u);
  check_eq(stats.failed, 1u);
  info("reset error count to not fail the test");
  caf::test::reporter::instance->test_stats({2, 0});
}

TEST("each run starts with fresh local variables") {
  auto my_int = 0;
  SECTION("block 1 reads my_int as 0") {
    check_eq(my_int, 0);
    my_int = 42;
    check_eq(my_int, 42);
  }
  SECTION("block 2 also reads my_int as 0") {
    check_eq(my_int, 0);
  }
}

struct int_fixture {
  int my_int = 0;
};

WITH_FIXTURE(int_fixture) {

TEST("each run starts with a fresh fixture") {
  SECTION("block 1 reads my_int as 0") {
    check_eq(my_int, 0);
    my_int = 42;
    check_eq(my_int, 42);
  }
  SECTION("block 2 also reads my_int as 0") {
    check_eq(my_int, 0);
  }
}

} // WITH_FIXTURE(int_fixture)

CAF_TEST_MAIN()
