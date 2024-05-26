// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/telemetry/label.hpp"
#include "caf/telemetry/metric_type.hpp"

#include <atomic>
#include <cstdint>

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

  explicit dbl_gauge(span<const label>) noexcept : value_(0) {
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
