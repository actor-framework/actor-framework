// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/test.hpp"

#include "caf/test/caf_test_main.hpp"

using caf::test::block_type;

TEST("tests can contain different types of checks") {
  auto* rep = caf::test::reporter::instance;
  auto stats = rep->test_stats();
  SECTION("checks functionality of check_ne") {
    check_ne(0u, 1u);
    should_fail([this]() { check_ne(0u, 0u); });
  }
  SECTION("checks functionality of check_eq") {
    check_eq(1u, 1u);
    should_fail([this]() { check_eq(1u, 0u); });
  }
  SECTION("checks functionality of check_ge") {
    check_ge(0u, 0u);
    check_ge(2u, 1u);
    should_fail([this]() { check_ge(1u, 2u); });
  }
  SECTION("checks functionality of check_gt") {
    check_gt(2u, 1u);
    should_fail([this]() { check_gt(0u, 0u); });
    should_fail([this]() { check_gt(1u, 2u); });
  }
  SECTION("checks functionality of check_le") {
    check_le(0u, 0u);
    check_le(1u, 2u);
    should_fail([this]() { check_le(2u, 1u); });
  }
  SECTION("checks functionality of check_lt") {
    check_lt(1u, 2u);
    should_fail([this]() { check_lt(1u, 1u); });
    should_fail([this]() { check_lt(2u, 1u); });
  }
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
