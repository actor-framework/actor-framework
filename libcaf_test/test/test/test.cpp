// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/test.hpp"

using caf::test::block_type;

SUITE("caf.test.test") {

TEST("tests can contain checks") {
  auto* rep = caf::test::reporter::instance;
  for (int i = 0; i < 3; ++i)
    check_eq(i, i);
  auto stats = rep->test_stats();
  check_eq(stats.passed, 3u);
  check_eq(stats.failed, 0u);
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

} // SUITE("caf.test.test")
