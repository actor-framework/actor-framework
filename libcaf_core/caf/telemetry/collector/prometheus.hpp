// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <ctime>
#include <unordered_map>
#include <vector>

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/string_view.hpp"
#include "caf/telemetry/counter.hpp"
#include "caf/telemetry/gauge.hpp"
#include "caf/telemetry/histogram.hpp"
#include "caf/timespan.hpp"
#include "caf/timestamp.hpp"

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
  [[nodiscard]] timespan min_scrape_interval() const noexcept {
    return min_scrape_interval_;
  }

  /// Sets the minimum scrape interval to `value`.
  void min_scrape_interval(timespan value) noexcept {
    min_scrape_interval_ = value;
  }

  /// Returns the time point of the last scrape.
  [[nodiscard]] timestamp last_scrape() const noexcept {
    return last_scrape_;
  }

  /// Returns a string view into the internal buffer.
  /// @warning This view may become invalid when calling any non-const member
  ///          function on the collector object.
  [[nodiscard]] string_view str() const noexcept {
    return {buf_.data(), buf_.size()};
  }

  /// Reverts the collector back to its initial state, clearing all buffers.
  void reset();

  // -- scraping API -----------------------------------------------------------

  /// Begins a new scrape if `last_scape() + min_scrape_interval() <= now`.
  /// @returns `true` if the collector started a new scrape or `false` to
  ///           signal that the caller shall use the last result via `str()`
  ///           since it has not expired yet.
  [[nodiscard]] bool begin_scrape(timestamp now = make_timestamp());

  /// Cleans up any temporary state before accessing `str()` for obtaining the
  /// scrape result.
  void end_scrape();

  // -- appending into the internal buffer -------------------------------------

  template <class T>
  void
  append_counter(const metric_family* family, const metric* instance, T value) {
    append_impl(family, "counter", instance, value);
  }

  template <class T>
  void
  append_gauge(const metric_family* family, const metric* instance, T value) {
    append_impl(family, "gauge", instance, value);
  }

  void append_histogram(const metric_family* family, const metric* instance,
                        span<const int_histogram::bucket_type> buckets,
                        int64_t sum);

  void append_histogram(const metric_family* family, const metric* instance,
                        span<const dbl_histogram::bucket_type> buckets,
                        double sum);

  // -- collect API ------------------------------------------------------------

  /// Applies this collector to the registry, filling the character buffer while
  /// collecting metrics. Automatically calls `begin_scrape` and `end_scrape` as
  /// needed.
  /// @param registry Source for the metrics.
  /// @param now Current system time.
  /// @returns a view into the filled buffer.
  string_view collect_from(const metric_registry& registry,
                           timestamp now = make_timestamp());

  // -- call operators for the metric registry ---------------------------------

  void operator()(const metric_family* family, const metric* instance,
                  const dbl_counter* counter) {
    append_counter(family, instance, counter->value());
  }

  void operator()(const metric_family* family, const metric* instance,
                  const int_counter* counter) {
    append_counter(family, instance, counter->value());
  }

  void operator()(const metric_family* family, const metric* instance,
                  const dbl_gauge* gauge) {
    append_gauge(family, instance, gauge->value());
  }

  void operator()(const metric_family* family, const metric* instance,
                  const int_gauge* gauge) {
    append_gauge(family, instance, gauge->value());
  }

  void operator()(const metric_family* family, const metric* instance,
                  const dbl_histogram* val) {
    append_histogram(family, instance, val->buckets(), val->sum());
  }

  void operator()(const metric_family* family, const metric* instance,
                  const int_histogram* val) {
    append_histogram(family, instance, val->buckets(), val->sum());
  }

private:
  // -- implementation details -------------------------------------------------

  /// Sets `current_family_` if not pointing to `family` already. When setting
  /// the member variable, also writes meta information to `buf_`.
  void set_current_family(const metric_family* family,
                          string_view prometheus_type);

  void append_impl(const metric_family* family, string_view prometheus_type,
                   const metric* instance, int64_t value);

  void append_impl(const metric_family* family, string_view prometheus_type,
                   const metric* instance, double value);

  template <class BucketType, class ValueType>
  void append_histogram_impl(const metric_family* family,
                             const metric* instance,
                             span<const BucketType> buckets, ValueType sum);

  // -- member variables -------------------------------------------------------

  /// Stores the generated text output.
  char_buffer buf_;

  /// Current timestamp.
  timestamp last_scrape_ = timestamp{timespan{0}};

  /// Caches type information and help text for a metric.
  std::unordered_map<const metric_family*, char_buffer> family_info_;

  /// Caches variable names for each bucket of a histogram as well as for the
  /// implicit sum and count fields.
  std::unordered_map<const metric*, std::vector<char_buffer>> histogram_info_;

  /// Caches which metric family is currently collected.
  const metric_family* current_family_ = nullptr;

  /// Minimum time between re-iterating the registry.
  timespan min_scrape_interval_ = timespan{0};
};

} // namespace caf::telemetry::collector
