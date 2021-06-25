// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf::telemetry::importer {

/// Imports CPU and memory metrics for the current process. On supported
/// platforms, this importer adds the metrics `process.resident_memory`
/// (resident memory size), `process.virtual_memory` (virtual memory size) and
/// `process.cpu` (total user and system CPU time spent).
///
/// @note CAF adds this importer automatically when configuring export to
///       Prometheus via HTTP.
class CAF_CORE_EXPORT process {
public:
  explicit process(metric_registry& reg);

  /// Returns whether the scraper supports the host platform.
  static bool platform_supported() noexcept;

  /// Updates process metrics.
  /// @note has no effect if `platform_supported()` returns `false`.
  void update();

private:
  telemetry::int_gauge* rss_ = nullptr;
  telemetry::int_gauge* vms_ = nullptr;
  telemetry::dbl_gauge* cpu_ = nullptr;
};

} // namespace caf::telemetry::importer
