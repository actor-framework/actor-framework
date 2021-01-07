// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <ctime>
#include <unordered_map>
#include <vector>

#include "caf/byte_buffer.hpp"
#include "caf/detail/io_export.hpp"
#include "caf/fwd.hpp"
#include "caf/io/broker.hpp"
#include "caf/telemetry/collector/prometheus.hpp"

namespace caf::detail {

/// Makes system metrics in the Prometheus format available via HTTP 1.1.
class CAF_IO_EXPORT prometheus_broker : public io::broker {
public:
  explicit prometheus_broker(actor_config& cfg);

  prometheus_broker(actor_config& cfg, io::doorman_ptr ptr);

  ~prometheus_broker() override;

  const char* name() const override;

  static bool has_process_metrics() noexcept;

  behavior make_behavior() override;

private:
  void scrape();

  std::unordered_map<io::connection_handle, byte_buffer> requests_;
  telemetry::collector::prometheus collector_;
  time_t last_scrape_ = 0;
  telemetry::dbl_gauge* cpu_time_ = nullptr;
  telemetry::int_gauge* mem_size_ = nullptr;
  telemetry::int_gauge* virt_mem_size_ = nullptr;
};

} // namespace caf::detail
