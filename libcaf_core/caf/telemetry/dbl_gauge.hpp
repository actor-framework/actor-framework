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

#include "caf/detail/core_export.hpp"

#include <atomic>
#include <cstdint>

#include "caf/telemetry/metric_type.hpp"

namespace caf::telemetry {

/// A metric that represents a single integer value that can arbitrarily go up
/// and down.
class CAF_CORE_EXPORT dbl_gauge {
public:
  // -- member types -----------------------------------------------------------

  using value_type = double;

  using family_setting = unit_t;

  // -- constants --------------------------------------------------------------

  static constexpr metric_type runtime_type = metric_type::dbl_gauge;

  // -- constructors, destructors, and assignment operators --------------------

  dbl_gauge() noexcept : value_(0) {
    // nop
  }

  explicit dbl_gauge(double value) noexcept : value_(value) {
    // nop
  }

  // -- modifiers --------------------------------------------------------------

  /// Increments the gauge by 1.
  void inc() noexcept {
    inc(1.0);
  }

  /// Increments the gauge by `amount`.
  void inc(double amount) noexcept {
    auto val = value_.load();
    auto new_val = val + amount;
    while (!value_.compare_exchange_weak(val, new_val)) {
      new_val = val + amount;
    }
  }

  /// Decrements the gauge by 1.
  void dec() noexcept {
    dec(1.0);
  }

  /// Decrements the gauge by `amount`.
  void dec(double amount) noexcept {
    inc(-amount);
  }

  /// Sets the gauge to `x`.
  void value(double x) noexcept {
    value_.store(x);
  }

  // -- observers --------------------------------------------------------------

  /// Returns the current value of the gauge.
  double value() const noexcept {
    return value_.load();
  }

private:
  std::atomic<double> value_;
};

} // namespace caf::telemetry
