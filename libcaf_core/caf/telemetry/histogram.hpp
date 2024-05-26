// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/assert.hpp"
#include "caf/fwd.hpp"
#include "caf/settings.hpp"
#include "caf/span.hpp"
#include "caf/telemetry/counter.hpp"
#include "caf/telemetry/gauge.hpp"
#include "caf/telemetry/label.hpp"
#include "caf/telemetry/metric_type.hpp"

#include <algorithm>
#include <type_traits>

namespace caf::telemetry {

/// Represent aggregatable distributions of events.
template <class ValueType>
class histogram {
public:
  // -- member types -----------------------------------------------------------

  using value_type = ValueType;

  using gauge_type = gauge<value_type>;

  using family_setting = std::vector<value_type>;

  struct bucket_type {
    value_type upper_bound;
    int_counter count;
  };

  // -- constants --------------------------------------------------------------

  static constexpr metric_type runtime_type = std::is_same_v<value_type, double>
                                                ? metric_type::dbl_histogram
                                                : metric_type::int_histogram;

  // -- constructors, destructors, and assignment operators --------------------

  histogram(span<const label> labels, const settings* cfg,
            span<const value_type> upper_bounds) {
    if (!init_buckets_from_config(labels, cfg))
      init_buckets(upper_bounds);
  }

  explicit histogram(std::initializer_list<value_type> upper_bounds)
    : histogram({}, nullptr,
                make_span(upper_bounds.begin(), upper_bounds.size())) {
    // nop
  }

  histogram(const histogram&) = delete;

  histogram& operator=(const histogram&) = delete;

  ~histogram() {
    delete[] buckets_;
  }

  // -- modifiers --------------------------------------------------------------

  /// Increments the bucket where the observed value falls into and increments
  /// the sum of all observed values.
  void observe(value_type value) {
    // The last bucket has an upper bound of +inf or int_max, so we'll always
    // find a bucket and increment the counters.
    for (size_t index = 0;; ++index) {
      auto& [upper_bound, count] = buckets_[index];
      if (value <= upper_bound) {
        count.inc();
        sum_.inc(value);
        return;
      }
    }
  }

  // -- observers --------------------------------------------------------------

  /// Returns the ``counter`` objects with the configured upper bounds.
  span<const bucket_type> buckets() const noexcept {
    return {buckets_, num_buckets_};
  }

  /// Returns the sum of all observed values.
  value_type sum() const noexcept {
    return sum_.value();
  }

private:
  void init_buckets(span<const value_type> upper_bounds) {
    CAF_ASSERT(std::is_sorted(upper_bounds.begin(), upper_bounds.end()));
    using limits = std::numeric_limits<value_type>;
    num_buckets_ = upper_bounds.size() + 1;
    buckets_ = new bucket_type[num_buckets_];
    size_t index = 0;
    for (; index < upper_bounds.size(); ++index)
      buckets_[index].upper_bound = upper_bounds[index];
    if constexpr (limits::has_infinity)
      buckets_[index].upper_bound = limits::infinity();
    else
      buckets_[index].upper_bound = limits::max();
  }

  bool init_buckets_from_config(span<const label> labels, const settings* cfg) {
    if (cfg == nullptr || labels.empty())
      return false;
    for (const auto& lbl : labels) {
      if (auto ptr = get_if<settings>(cfg, lbl.str())) {
        if (auto bounds = get_as<std::vector<value_type>>(*ptr, "buckets")) {
          std::sort(bounds->begin(), bounds->end());
          bounds->erase(std::unique(bounds->begin(), bounds->end()),
                        bounds->end());
          if (bounds->empty())
            return false;
          init_buckets(*bounds);
          return true;
        }
      }
    }
    return false;
  }

  size_t num_buckets_;
  bucket_type* buckets_;
  gauge_type sum_;
};

/// Convenience alias for a histogram with value type `double`.
using dbl_histogram = histogram<double>;

/// Convenience alias for a histogram with value type `int64_t`.
using int_histogram = histogram<int64_t>;

} // namespace caf::telemetry
