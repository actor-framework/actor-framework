/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

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
