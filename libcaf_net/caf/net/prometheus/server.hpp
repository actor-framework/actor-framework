// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/fwd.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/http/upper_layer.hpp"
#include "caf/telemetry/collector/prometheus.hpp"
#include "caf/telemetry/importer/process.hpp"

#include <chrono>
#include <memory>
#include <string_view>

namespace caf::net::prometheus {

/// Makes metrics available to clients via the Prometheus exposition format.
class server : public http::upper_layer {
public:
  // -- member types -----------------------------------------------------------

  /// State for scraping Metrics data. Shared between all server instances.
  class scrape_state {
  public:
    using clock_type = std::chrono::steady_clock;
    using time_point = clock_type::time_point;
    using duration = clock_type::duration;

    std::string_view scrape();

    explicit scrape_state(telemetry::metric_registry* ptr)
      : registry(ptr), last_scrape(duration{0}), proc_importer(*ptr) {
      // nop
    }

    static std::shared_ptr<scrape_state> make(telemetry::metric_registry* ptr) {
      return std::make_shared<scrape_state>(ptr);
    }

    telemetry::metric_registry* registry;
    std::chrono::steady_clock::time_point last_scrape;
    telemetry::importer::process proc_importer;
    telemetry::collector::prometheus collector;
  };

  using scrape_state_ptr = std::shared_ptr<scrape_state>;

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<server> make(scrape_state_ptr state) {
    return std::unique_ptr<server>{new server(std::move(state))};
  }

  // -- implementation of http::upper_layer ------------------------------------

  void prepare_send() override;

  bool done_sending() override;

  void abort(const error& reason) override;

  error start(http::lower_layer* down) override;

  ptrdiff_t consume(const http::header& hdr, const_byte_span payload) override;

private:
  explicit server(scrape_state_ptr state) : state_(std::move(state)) {
    // nop
  }

  scrape_state_ptr state_;
  http::lower_layer* down_ = nullptr;
};

} // namespace caf::net::prometheus
