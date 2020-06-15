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

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/string_view.hpp"

namespace caf::telemetry::collector {

/// Collects system metrics and exports them to the text-based Prometheus
/// format. For a documentation of the format, see: https://git.io/fjgDD.
class CAF_CORE_EXPORT prometheus {
public:
  // -- member types -----------------------------------------------------------

  /// A buffer for storing UTF-8 characters. Using a vector instead of a
  /// `std::string` has slight performance benefits, since the vector does not
  /// have to maintain a null-terminator.
  using char_buffer = std::vector<char>;

  // -- properties -------------------------------------------------------------

  /// Returns the minimum scrape interval, i.e., the minimum time that needs to
  /// pass before `collect_from` iterates the registry to re-fill the buffer.
  time_t min_scrape_interval() const noexcept {
    return min_scrape_interval_;
  }

  /// Sets the minimum scrape interval to `value`.
  void min_scrape_interval(time_t value) noexcept {
    min_scrape_interval_ = value;
  }

  // -- collect API ------------------------------------------------------------

  /// Applies this collector to the registry, filling the character buffer while
  /// collecting metrics.
  /// @param registry Source for the metrics.
  /// @param now Timestamp as time since UNIX epoch in seconds.
  /// @returns a view into the filled buffer.
  string_view collect_from(const metric_registry& registry, time_t now);

  /// Applies this collector to the registry, filling the character buffer while
  /// collecting metrics. Uses the current system time as timestamp.
  /// @param registry Source for the metrics.
  /// @returns a view into the filled buffer.
  string_view collect_from(const metric_registry& registry);

  // -- call operators for the metric registry ---------------------------------

  void operator()(const metric_family* family, const metric* instance,
                  const dbl_gauge* gauge);

  void operator()(const metric_family* family, const metric* instance,
                  const int_gauge* gauge);

private:
  /// Sets `current_family_` if not pointing to `family` already. When setting
  /// the member variable, also writes meta information to `buf_`.
  void set_current_family(const metric_family* family,
                          string_view prometheus_type);

  /// Stores the generated text output.
  char_buffer buf_;

  /// Current timestamp.
  time_t now_ = 0;

  /// Caches type information and help text for a metric.
  std::unordered_map<const metric_family*, char_buffer> meta_info_;

  /// Caches which metric family is currently collected.
  const metric_family* current_family_ = nullptr;

  /// Minimum time between re-iterating the registry.
  time_t min_scrape_interval_ = 0;
};

} // namespace caf::telemetry::collector
