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

#include <algorithm>
#include <type_traits>

#include "caf/config.hpp"
#include "caf/fwd.hpp"
#include "caf/span.hpp"
#include "caf/telemetry/gauge.hpp"
#include "caf/telemetry/metric_type.hpp"

namespace caf::telemetry {

/// Represent aggregatable distributions of events.
template <class ValueType>
class histogram {
public:
  // -- member types -----------------------------------------------------------

  using value_type = ValueType;

  using gauge_type = gauge<value_type>;

  struct bucket_type {
    value_type upper_bound;
    gauge_type gauge;
  };

  // -- constants --------------------------------------------------------------

  static constexpr metric_type runtime_type
    = std::is_same<value_type, double>::value ? metric_type::dbl_histogram
                                              : metric_type::int_histogram;

  // -- constructors, destructors, and assignment operators --------------------

  histogram(span<const value_type> upper_bounds) {
    using limits = std::numeric_limits<value_type>;
    CAF_ASSERT(std::is_sorted(upper_bounds.begin(), upper_bounds.end()));
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

  histogram(std::initializer_list<value_type> upper_bounds)
    : histogram(make_span(upper_bounds.begin(), upper_bounds.size())) {
    // nop
  }

  ~histogram() {
    delete[] buckets_;
  }

  // -- modifiers --------------------------------------------------------------

  void observe(value_type value) {
    // The last bucket has an upper bound of +inf or int_max, so we'll always
    // find a bucket and increment the gauges.
    for (size_t index = 0;; ++index) {
      auto& [upper_bound, gauge] = buckets_[index];
      if (value <= upper_bound) {
        gauge.inc();
        sum_.inc(value);
        return;
      }
    }
  }

  // -- observers --------------------------------------------------------------

  span<const bucket_type> buckets() const noexcept {
    return {buckets_, num_buckets_};
  }

  value_type sum() const noexcept {
    return sum_.value();
  }

private:
  size_t num_buckets_;
  bucket_type* buckets_;
  gauge_type sum_;
};

///
using dbl_histogram = histogram<double>;

using int_histogram = histogram<int64_t>;

} // namespace caf::telemetry
