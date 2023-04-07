// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/prometheus.hpp"

#include "caf/net/http/lower_layer.hpp"
#include "caf/net/http/request_header.hpp"

using namespace std::literals;

namespace caf::net::prometheus {

std::string_view scrape_state::scrape() {
  if (auto now = std::chrono::steady_clock::now();
      last_scrape + proc_import_interval <= now) {
    last_scrape = now;
    proc_importer.update();
  }
  return collector.collect_from(*registry);
}

} // namespace caf::net::prometheus
