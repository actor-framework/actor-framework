// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/telemetry/importer/process.hpp"

#include "caf/test/approx.hpp"
#include "caf/test/test.hpp"

#include "caf/telemetry/metric_registry.hpp"

using namespace caf;
using namespace caf::telemetry;

TEST("platform_supported returns true on macOS, Linux, and NetBSD") {
  bool supported = telemetry::importer::process::platform_supported();
#if defined(CAF_MACOS) || defined(CAF_LINUX) || defined(CAF_NET_BSD)
  check(supported);
#else
  check(!supported);
#endif
}

TEST("creating a process importer registers process metrics") {
  metric_registry reg;
  telemetry::importer::process importer{reg};
  if (telemetry::importer::process::platform_supported()) {
    check_ne(importer.rss(), nullptr);
    check_ne(importer.vms(), nullptr);
    check_ne(importer.cpu(), nullptr);
    check_ne(importer.fds(), nullptr);
  } else {
    check_eq(importer.rss(), nullptr);
    check_eq(importer.vms(), nullptr);
    check_eq(importer.cpu(), nullptr);
    check_eq(importer.fds(), nullptr);
  }
}

TEST("update sets non-negative values for all process metrics") {
  metric_registry reg;
  telemetry::importer::process importer{reg};
  if (telemetry::importer::process::platform_supported()) {
    auto rss = importer.rss();
    auto vms = importer.vms();
    auto cpu = importer.cpu();
    auto fds = importer.fds();
    SECTION("initial values are zero before the first update") {
      check_eq(rss->value(), 0);
      check_eq(vms->value(), 0);
      check_eq(cpu->value(), test::approx{0.0});
      check_eq(fds->value(), 0);
    }
    SECTION("after update, all metrics have non-negative values") {
      importer.update();
      check_ge(rss->value(), 0);
      check_ge(vms->value(), 0);
      check_ge(cpu->value(), 0.0);
      check_ge(fds->value(), 0);
    }
  }
}
