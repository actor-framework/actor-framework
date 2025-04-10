// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/http/responder.hpp"

#include "caf/actor_system.hpp"
#include "caf/fwd.hpp"
#include "caf/telemetry/collector/prometheus.hpp"
#include "caf/telemetry/importer/process.hpp"

#include <chrono>
#include <memory>
#include <string_view>

namespace caf::net::prometheus {

/// State for scraping Metrics data. May be shared among scrapers as long as
/// they don't access the state concurrently.
class CAF_NET_EXPORT scrape_state {
public:
  using clock_type = std::chrono::steady_clock;
  using time_point = clock_type::time_point;
  using duration = clock_type::duration;

  std::string_view scrape();

  scrape_state(telemetry::metric_registry* ptr, timespan proc_import_interval)
    : registry(ptr),
      last_scrape(duration{0}),
      proc_import_interval(proc_import_interval),
      proc_importer(*ptr) {
    // nop
  }

  telemetry::metric_registry* registry;
  std::chrono::steady_clock::time_point last_scrape;
  timespan proc_import_interval;
  telemetry::importer::process proc_importer;
  telemetry::collector::prometheus collector;
};

/// Creates a scraper for the given actor system.
/// @param registry The registry for collecting the metrics from.
/// @param proc_import_interval Minimum time between importing process metrics.
/// @return a function object suitable for passing it to an HTTP route.
inline auto scraper(telemetry::metric_registry* registry,
                    timespan proc_import_interval = std::chrono::seconds{1}) {
  auto state = std::make_shared<scrape_state>(registry, proc_import_interval);
  return [state](http::responder& res) {
    res.respond(http::status::ok, "text/plain;version=0.0.4", state->scrape());
  };
}

/// Creates a scraper for the given actor system.
/// @param sys The actor system for collecting the metrics from.
/// @param proc_import_interval Minimum time between importing process metrics.
/// @returns a function object suitable for passing it to an HTTP route.
inline auto scraper(actor_system& sys,
                    timespan proc_import_interval = std::chrono::seconds{1}) {
  return scraper(&sys.metrics(), proc_import_interval);
}

} // namespace caf::net::prometheus
