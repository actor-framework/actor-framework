// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/test.hpp"

#include "caf/test/approx.hpp"
#include "caf/test/requirement_failed.hpp"

#include "caf/log/test.hpp"
#include "caf/telemetry/metric_registry.hpp"

#include <chrono>

using namespace std::literals;

namespace {

constexpr auto apx1 = caf::test::approx{1.0};
constexpr auto apx7 = caf::test::approx{7.0};

} // namespace

TEST("tests can contain different types of checks") {
  auto& rep = caf::test::reporter::instance();
  SECTION("check_ne checks for inequality") {
    check_ne(0, 1);
    should_fail([this] { check_ne(0, 0); });
  }
  SECTION("check_eq checks for equality") {
    check_eq(1, 1);
    should_fail([this] { check_eq(1, 0); });
  }
  SECTION("check_ge checks that lhs is greater than or equal to rhs") {
    check_ge(0, 0);
    check_ge(2, 1);
    should_fail([this] { check_ge(1, 2); });
  }
  SECTION("check_gt checks that lhs is greater than rhs") {
    check_gt(2, 1);
    should_fail([this] { check_gt(0, 0); });
    should_fail([this] { check_gt(1, 2); });
  }
  SECTION("check_le checks that lhs is less than or equal to rhs") {
    check_le(0, 0);
    check_le(1, 2);
    should_fail([this] { check_le(2, 1); });
  }
  SECTION("check_lt checks that lhs is less than rhs") {
    check_lt(1, 2);
    should_fail([this] { check_lt(1, 1); });
    should_fail([this] { check_lt(2, 1); });
  }
  caf::log::test::debug("this test had {} checks", rep.test_stats().total());
}

#ifdef CAF_ENABLE_EXCEPTIONS

TEST("all metric checks throw when not setting a registry") {
  check_throws([this] { check_metric_eq("tst", "tst", 1); });
  check_throws([this] { check_metric_eq("tst", "tst", {{"a", "b"}}, 1); });
  check_throws([this] { check_metric_eq("tst", "tst", apx1); });
  check_throws([this] { check_metric_eq("tst", "tst", {{"a", "b"}}, apx1); });
  check_throws([this] { check_metric_ne("tst", "tst", 1); });
  check_throws([this] { check_metric_ne("tst", "tst", {{"a", "b"}}, 1); });
  check_throws([this] { check_metric_ne("tst", "tst", apx1); });
  check_throws([this] { check_metric_ne("tst", "tst", {{"a", "b"}}, apx1); });
  check_throws([this] { check_metric_lt("tst", "tst", 1); });
  check_throws([this] { check_metric_lt("tst", "tst", {{"a", "b"}}, 1); });
  check_throws([this] { check_metric_lt("tst", "tst", 1.0); });
  check_throws([this] { check_metric_lt("tst", "tst", {{"a", "b"}}, 1.0); });
  check_throws([this] { check_metric_le("tst", "tst", 1); });
  check_throws([this] { check_metric_le("tst", "tst", {{"a", "b"}}, 1); });
  check_throws([this] { check_metric_le("tst", "tst", 1.0); });
  check_throws([this] { check_metric_le("tst", "tst", {{"a", "b"}}, 1.0); });
  check_throws([this] { check_metric_gt("tst", "tst", 1); });
  check_throws([this] { check_metric_gt("tst", "tst", {{"a", "b"}}, 1); });
  check_throws([this] { check_metric_gt("tst", "tst", 1.0); });
  check_throws([this] { check_metric_gt("tst", "tst", {{"a", "b"}}, 1.0); });
  check_throws([this] { check_metric_ge("tst", "tst", 1); });
  check_throws([this] { check_metric_ge("tst", "tst", {{"a", "b"}}, 1); });
  check_throws([this] { check_metric_ge("tst", "tst", 1.0); });
  check_throws([this] { check_metric_ge("tst", "tst", {{"a", "b"}}, 1.0); });
}

TEST("all metric requirements throw when not setting a registry") {
  check_throws([this] { require_metric_eq("tst", "tst", 1); });
  check_throws([this] { require_metric_eq("tst", "tst", {{"a", "b"}}, 1); });
  check_throws([this] { require_metric_eq("tst", "tst", apx1); });
  check_throws([this] { require_metric_eq("tst", "tst", {{"a", "b"}}, apx1); });
  check_throws([this] { require_metric_ne("tst", "tst", 1); });
  check_throws([this] { require_metric_ne("tst", "tst", {{"a", "b"}}, 1); });
  check_throws([this] { require_metric_ne("tst", "tst", apx1); });
  check_throws([this] { require_metric_ne("tst", "tst", {{"a", "b"}}, apx1); });
  check_throws([this] { require_metric_lt("tst", "tst", 1); });
  check_throws([this] { require_metric_lt("tst", "tst", {{"a", "b"}}, 1); });
  check_throws([this] { require_metric_lt("tst", "tst", 1.0); });
  check_throws([this] { require_metric_lt("tst", "tst", {{"a", "b"}}, 1.0); });
  check_throws([this] { require_metric_le("tst", "tst", 1); });
  check_throws([this] { require_metric_le("tst", "tst", {{"a", "b"}}, 1); });
  check_throws([this] { require_metric_le("tst", "tst", 1.0); });
  check_throws([this] { require_metric_le("tst", "tst", {{"a", "b"}}, 1.0); });
  check_throws([this] { require_metric_gt("tst", "tst", 1); });
  check_throws([this] { require_metric_gt("tst", "tst", {{"a", "b"}}, 1); });
  check_throws([this] { require_metric_gt("tst", "tst", 1.0); });
  check_throws([this] { require_metric_gt("tst", "tst", {{"a", "b"}}, 1.0); });
  check_throws([this] { require_metric_ge("tst", "tst", 1); });
  check_throws([this] { require_metric_ge("tst", "tst", {{"a", "b"}}, 1); });
  check_throws([this] { require_metric_ge("tst", "tst", 1.0); });
  check_throws([this] { require_metric_ge("tst", "tst", {{"a", "b"}}, 1.0); });
}

TEST("tests fail when requirement errors occur") {
  using caf::test::requirement_failed;
  auto& rep = caf::test::reporter::instance();
  SECTION("require_eq fails when lhs != rhs") {
    should_fail_with_exception<requirement_failed>(
      [this] { require_eq(1, 2); });
    require_eq(1, 1);
  }
  SECTION("require_ne fails when lhs == rhs") {
    should_fail_with_exception<requirement_failed>(
      [this] { require_ne(1, 1); });
    require_ne(1, 2);
  }
  SECTION("require_le fails when lhs > rhs") {
    should_fail_with_exception<requirement_failed>(
      [this] { require_le(2, 1); });
    require_le(1, 2);
    require_le(2, 2);
  }
  SECTION("require_lt fails when lhs >= rhs") {
    should_fail_with_exception<requirement_failed>(
      [this] { require_lt(2, 2); });
    should_fail_with_exception<requirement_failed>(
      [this] { require_lt(2, 1); });
    require_lt(1, 2);
  }
  SECTION("require_ge fails when lhs < rhs") {
    should_fail_with_exception<requirement_failed>(
      [this] { require_ge(1, 2); });
    require_ge(2, 1);
    require_ge(2, 2);
  }
  SECTION("require_gt fails when lhs <= rhs") {
    should_fail_with_exception<requirement_failed>(
      [this] { require_gt(1, 1); });
    should_fail_with_exception<requirement_failed>(
      [this] { require_gt(1, 2); });
    require_gt(2, 1);
  }
  caf::log::test::debug("this test had {} checks", rep.test_stats().total());
}

#endif // CAF_ENABLE_EXCEPTIONS

TEST("all metric checks fail if the metric does not exist") {
  caf::telemetry::metric_registry reg;
  current_metric_registry(&reg);
  metric_registry_poll_interval(1ns);
  metric_registry_poll_timeout(1ns);
  should_fail([this] { check_metric_eq("tst", "tst", 1); });
  should_fail([this] { check_metric_eq("tst", "tst", {{"a", "b"}}, 1); });
  should_fail([this] { check_metric_eq("tst", "tst", apx1); });
  should_fail([this] { check_metric_eq("tst", "tst", {{"a", "b"}}, apx1); });
  should_fail([this] { check_metric_ne("tst", "tst", 1); });
  should_fail([this] { check_metric_ne("tst", "tst", {{"a", "b"}}, 1); });
  should_fail([this] { check_metric_ne("tst", "tst", apx1); });
  should_fail([this] { check_metric_ne("tst", "tst", {{"a", "b"}}, apx1); });
  should_fail([this] { check_metric_lt("tst", "tst", 1); });
  should_fail([this] { check_metric_lt("tst", "tst", {{"a", "b"}}, 1); });
  should_fail([this] { check_metric_lt("tst", "tst", 1.0); });
  should_fail([this] { check_metric_lt("tst", "tst", {{"a", "b"}}, 1.0); });
  should_fail([this] { check_metric_le("tst", "tst", 1); });
  should_fail([this] { check_metric_le("tst", "tst", {{"a", "b"}}, 1); });
  should_fail([this] { check_metric_le("tst", "tst", 1.0); });
  should_fail([this] { check_metric_le("tst", "tst", {{"a", "b"}}, 1.0); });
  should_fail([this] { check_metric_gt("tst", "tst", 1); });
  should_fail([this] { check_metric_gt("tst", "tst", {{"a", "b"}}, 1); });
  should_fail([this] { check_metric_gt("tst", "tst", 1.0); });
  should_fail([this] { check_metric_gt("tst", "tst", {{"a", "b"}}, 1.0); });
  should_fail([this] { check_metric_ge("tst", "tst", 1); });
  should_fail([this] { check_metric_ge("tst", "tst", {{"a", "b"}}, 1); });
  should_fail([this] { check_metric_ge("tst", "tst", 1.0); });
  should_fail([this] { check_metric_ge("tst", "tst", {{"a", "b"}}, 1.0); });
}

#ifdef CAF_ENABLE_EXCEPTIONS

TEST(
  "all require_metric_* throw requirement_failed when metric does not exist") {
  using caf::test::requirement_failed;
  caf::telemetry::metric_registry reg;
  current_metric_registry(&reg);
  metric_registry_poll_interval(1ns);
  metric_registry_poll_timeout(1ns);
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_eq("tst", "tst", 1); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_eq("tst", "tst", {{"a", "b"}}, 1); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_eq("tst", "tst", apx1); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_eq("tst", "tst", {{"a", "b"}}, apx1); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_ne("tst", "tst", 1); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_ne("tst", "tst", {{"a", "b"}}, 1); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_ne("tst", "tst", apx1); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_ne("tst", "tst", {{"a", "b"}}, apx1); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_lt("tst", "tst", 1); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_lt("tst", "tst", {{"a", "b"}}, 1); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_lt("tst", "tst", 1.0); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_lt("tst", "tst", {{"a", "b"}}, 1.0); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_le("tst", "tst", 1); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_le("tst", "tst", {{"a", "b"}}, 1); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_le("tst", "tst", 1.0); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_le("tst", "tst", {{"a", "b"}}, 1.0); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_gt("tst", "tst", 1); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_gt("tst", "tst", {{"a", "b"}}, 1); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_gt("tst", "tst", 1.0); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_gt("tst", "tst", {{"a", "b"}}, 1.0); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_ge("tst", "tst", 1); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_ge("tst", "tst", {{"a", "b"}}, 1); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_ge("tst", "tst", 1.0); });
  should_fail_with_exception<requirement_failed>(
    [this] { require_metric_ge("tst", "tst", {{"a", "b"}}, 1.0); });
}

#endif // CAF_ENABLE_EXCEPTIONS

TEST("metric checks pass if the metric exists with a matching value") {
  caf::telemetry::metric_registry reg;
  current_metric_registry(&reg);
  metric_registry_poll_interval(1ns);
  metric_registry_poll_timeout(1ns);
  SECTION("int gauge with no labels") {
    reg.gauge_singleton("tst", "tst", "test")->inc(7);
    check_metric_eq("tst", "tst", 7);
    check_metric_ne("tst", "tst", 0);
    check_metric_lt("tst", "tst", 8);
    check_metric_le("tst", "tst", 8);
    check_metric_gt("tst", "tst", 6);
    check_metric_ge("tst", "tst", 6);
    should_fail([this] { check_metric_eq("tst", "tst", {{"a", "c"}}, 7); });
  }
  SECTION("int gauge with labels") {
    reg.gauge_family("tst", "tst", {"a"}, "test")
      ->get_or_add({{"a", "b"}})
      ->inc(7);
    check_metric_eq("tst", "tst", {{"a", "b"}}, 7);
    check_metric_ne("tst", "tst", {{"a", "b"}}, 0);
    check_metric_lt("tst", "tst", {{"a", "b"}}, 8);
    check_metric_le("tst", "tst", {{"a", "b"}}, 8);
    check_metric_gt("tst", "tst", {{"a", "b"}}, 6);
    check_metric_ge("tst", "tst", {{"a", "b"}}, 6);
    should_fail([this] { check_metric_eq("tst", "tst", 7); });
    should_fail([this] { check_metric_eq("tst", "tst", {{"a", "c"}}, 7); });
  }
  SECTION("int counter with no labels") {
    reg.counter_singleton("tst", "cnt_no_lbl", "test")->inc(7);
    check_metric_eq("tst", "cnt_no_lbl", 7);
    check_metric_ne("tst", "cnt_no_lbl", 0);
    check_metric_lt("tst", "cnt_no_lbl", 8);
    check_metric_le("tst", "cnt_no_lbl", 8);
    check_metric_gt("tst", "cnt_no_lbl", 6);
    check_metric_ge("tst", "cnt_no_lbl", 6);
    should_fail(
      [this] { check_metric_eq("tst", "cnt_no_lbl", {{"a", "c"}}, 7); });
  }
  SECTION("int counter with labels") {
    reg.counter_family("tst", "tst", {"a"}, "test")
      ->get_or_add({{"a", "b"}})
      ->inc(7);
    check_metric_eq("tst", "tst", {{"a", "b"}}, 7);
    check_metric_ne("tst", "tst", {{"a", "b"}}, 0);
    check_metric_lt("tst", "tst", {{"a", "b"}}, 8);
    check_metric_le("tst", "tst", {{"a", "b"}}, 8);
    check_metric_gt("tst", "tst", {{"a", "b"}}, 6);
    check_metric_ge("tst", "tst", {{"a", "b"}}, 6);
    should_fail([this] { check_metric_eq("tst", "tst", 7); });
    should_fail([this] { check_metric_eq("tst", "tst", {{"a", "c"}}, 7); });
  }
  SECTION("double gauge with no labels") {
    reg.gauge_singleton<double>("tst", "tst", "test")->inc(7.0);
    check_metric_eq("tst", "tst", apx7);
    check_metric_ne("tst", "tst", apx1);
    check_metric_lt("tst", "tst", 8.0);
    check_metric_le("tst", "tst", 8.0);
    check_metric_gt("tst", "tst", 6.0);
    check_metric_ge("tst", "tst", 6.0);
    should_fail([this] { check_metric_eq("tst", "tst", {{"a", "c"}}, apx7); });
  }
  SECTION("double gauge with labels") {
    reg.gauge_family<double>("tst", "tst", {"a"}, "test")
      ->get_or_add({{"a", "b"}})
      ->inc(7.0);
    check_metric_eq("tst", "tst", {{"a", "b"}}, apx7);
    check_metric_ne("tst", "tst", {{"a", "b"}}, apx1);
    check_metric_lt("tst", "tst", {{"a", "b"}}, 8.0);
    check_metric_le("tst", "tst", {{"a", "b"}}, 8.0);
    check_metric_gt("tst", "tst", {{"a", "b"}}, 6.0);
    check_metric_ge("tst", "tst", {{"a", "b"}}, 6.0);
    should_fail([this] { check_metric_eq("tst", "tst", apx7); });
    should_fail([this] { check_metric_eq("tst", "tst", {{"a", "c"}}, apx7); });
  }
  SECTION("double counter with no labels") {
    reg.counter_singleton<double>("tst", "tst", "test")->inc(7.0);
    check_metric_eq("tst", "tst", apx7);
    check_metric_ne("tst", "tst", apx1);
    check_metric_lt("tst", "tst", 8.0);
    check_metric_le("tst", "tst", 8.0);
    check_metric_gt("tst", "tst", 6.0);
    check_metric_ge("tst", "tst", 6.0);
    should_fail([this] { check_metric_eq("tst", "tst", {{"a", "c"}}, apx7); });
  }
  SECTION("double counter with labels") {
    reg.counter_family<double>("tst", "tst", {"a"}, "test")
      ->get_or_add({{"a", "b"}})
      ->inc(7.0);
    check_metric_eq("tst", "tst", {{"a", "b"}}, apx7);
    check_metric_ne("tst", "tst", {{"a", "b"}}, apx1);
    check_metric_lt("tst", "tst", {{"a", "b"}}, 8.0);
    check_metric_le("tst", "tst", {{"a", "b"}}, 8.0);
    check_metric_gt("tst", "tst", {{"a", "b"}}, 6.0);
    check_metric_ge("tst", "tst", {{"a", "b"}}, 6.0);
    should_fail([this] { check_metric_eq("tst", "tst", apx7); });
    should_fail([this] { check_metric_eq("tst", "tst", {{"a", "c"}}, apx7); });
  }
  SECTION("approx: double gauge with no labels") {
    reg.gauge_singleton<double>("tst", "tst", "test")->inc(7.0);
    check_metric_eq("tst", "tst", apx7);
    should_fail([this] { check_metric_eq("tst", "tst", apx1); });
  }
  SECTION("approx: double gauge with labels") {
    reg.gauge_family<double>("tst", "tst", {"a"}, "test")
      ->get_or_add({{"a", "b"}})
      ->inc(7.0);
    check_metric_eq("tst", "tst", {{"a", "b"}}, apx7);
    should_fail([this] { check_metric_eq("tst", "tst", {{"a", "b"}}, apx1); });
  }
}

TEST("require_metric_* pass if the metric exists with a matching value") {
  caf::telemetry::metric_registry reg;
  current_metric_registry(&reg);
  metric_registry_poll_interval(1ns);
  metric_registry_poll_timeout(1ns);
  SECTION("int gauge with no labels") {
    reg.gauge_singleton("tst", "tst", "test")->inc(7);
    require_metric_eq("tst", "tst", 7);
    require_metric_ne("tst", "tst", 0);
    require_metric_lt("tst", "tst", 8);
    require_metric_le("tst", "tst", 8);
    require_metric_gt("tst", "tst", 6);
    require_metric_ge("tst", "tst", 6);
  }
  SECTION("int gauge with labels") {
    reg.gauge_family("tst", "tst", {"a"}, "test")
      ->get_or_add({{"a", "b"}})
      ->inc(7);
    require_metric_eq("tst", "tst", {{"a", "b"}}, 7);
    require_metric_ne("tst", "tst", {{"a", "b"}}, 0);
    require_metric_lt("tst", "tst", {{"a", "b"}}, 8);
    require_metric_le("tst", "tst", {{"a", "b"}}, 8);
    require_metric_gt("tst", "tst", {{"a", "b"}}, 6);
    require_metric_ge("tst", "tst", {{"a", "b"}}, 6);
  }
  SECTION("double gauge with no labels") {
    reg.gauge_singleton<double>("tst", "tst", "test")->inc(7.0);
    require_metric_eq("tst", "tst", apx7);
    require_metric_ne("tst", "tst", apx1);
    require_metric_lt("tst", "tst", 8.0);
    require_metric_le("tst", "tst", 8.0);
    require_metric_gt("tst", "tst", 6.0);
    require_metric_ge("tst", "tst", 6.0);
  }
  SECTION("double gauge with labels") {
    reg.gauge_family<double>("tst", "tst", {"a"}, "test")
      ->get_or_add({{"a", "b"}})
      ->inc(7.0);
    require_metric_eq("tst", "tst", {{"a", "b"}}, apx7);
    require_metric_ne("tst", "tst", {{"a", "b"}}, apx1);
    require_metric_lt("tst", "tst", {{"a", "b"}}, 8.0);
    require_metric_le("tst", "tst", {{"a", "b"}}, 8.0);
    require_metric_gt("tst", "tst", {{"a", "b"}}, 6.0);
    require_metric_ge("tst", "tst", {{"a", "b"}}, 6.0);
  }
}

#ifdef CAF_ENABLE_EXCEPTIONS

TEST("require_metric_* throw requirement_failed on a mismatch") {
  using caf::test::requirement_failed;
  caf::telemetry::metric_registry reg;
  current_metric_registry(&reg);
  metric_registry_poll_interval(1ns);
  metric_registry_poll_timeout(1ns);
  reg.gauge_singleton("tst", "tst", "test")->inc(7);
  should_fail_with_exception<requirement_failed>([this] { //
    require_metric_eq("tst", "tst", 0);
  });
  should_fail_with_exception<requirement_failed>([this] { //
    require_metric_eq("tst", "tst", 8);
  });
  should_fail_with_exception<requirement_failed>([this] { //
    require_metric_ne("tst", "tst", 7);
  });
  should_fail_with_exception<requirement_failed>([this] { //
    require_metric_lt("tst", "tst", 6);
  });
  should_fail_with_exception<requirement_failed>([this] { //
    require_metric_lt("tst", "tst", 7);
  });
  should_fail_with_exception<requirement_failed>([this] { //
    require_metric_le("tst", "tst", 6);
  });
  should_fail_with_exception<requirement_failed>([this] { //
    require_metric_gt("tst", "tst", 8);
  });
  should_fail_with_exception<requirement_failed>([this] { //
    require_metric_gt("tst", "tst", 7);
  });
  should_fail_with_exception<requirement_failed>([this] { //
    require_metric_ge("tst", "tst", 8);
  });
  require_metric_eq("tst", "tst", 7);
}

#endif // CAF_ENABLE_EXCEPTIONS

TEST("failed checks increment the failed counter") {
  auto& rep = caf::test::reporter::instance();
  auto lvl = rep.verbosity(caf::log::level::quiet);
  auto before = rep.test_stats();
  {
    caf::detail::scope_guard lvl_guard([&rep, lvl]() noexcept { //
      rep.verbosity(lvl);
    });
    check_eq(1, 2);
  }
  auto after = rep.test_stats();
  rep.test_stats(before);
  check_eq(before.passed, after.passed);
  check_eq(before.failed + 1, after.failed);
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

namespace {

struct int_fixture {
  int my_int = 0;
};

} // namespace

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
