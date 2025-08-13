// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/fwd.hpp"
#include "caf/telemetry/label.hpp"
#include "caf/telemetry/metric_type.hpp"

#include <atomic>
#include <cstdint>
#include <span>

namespace caf::telemetry {

/// A metric that represents a single integer value that can arbitrarily go up
/// and down.
template <class ValueType>
class gauge {
public:
  // -- member types -----------------------------------------------------------

  using value_type = ValueType;

  using family_setting = unit_t;

  // -- constants --------------------------------------------------------------

  static constexpr bool has_int_value = std::is_same_v<value_type, int64_t>;

  static constexpr metric_type runtime_type
    = has_int_value ? metric_type::int_gauge : metric_type::dbl_gauge;

  // -- constructors, destructors, and assignment operators --------------------

  gauge() noexcept : value_(0) {
    // nop
  }

  explicit gauge(value_type value) noexcept : value_(value) {
    // nop
  }

  explicit gauge(std::span<const label>) noexcept : value_(0) {
    // nop
  }

  // -- modifiers --------------------------------------------------------------

  /// Increments the gauge by 1.
  void inc() noexcept {
    if constexpr (has_int_value) {
      ++value_;
    } else {
      inc(1.0);
    }
  }

  /// Increments the gauge by `amount`.
  void inc(value_type amount) noexcept {
    if constexpr (has_int_value) {
      value_.fetch_add(amount);
    } else {
      auto val = value_.load();
      auto new_val = val + amount;
      while (!value_.compare_exchange_weak(val, new_val)) {
        new_val = val + amount;
      }
    }
  }

  /// Decrements the gauge by 1.
  void dec() noexcept {
    if constexpr (has_int_value) {
      --value_;
    } else {
      inc(-1.0);
    }
  }

  /// Decrements the gauge by `amount`.
  void dec(value_type amount) noexcept {
    if constexpr (has_int_value) {
      value_.fetch_sub(amount);
    } else {
      inc(-amount);
    }
  }

  /// Sets the gauge to `x`.
  void value(value_type x) noexcept {
    value_.store(x);
  }

  /// Increments the gauge by 1.
  /// @returns The new value of the gauge.
  value_type operator++() noexcept
    requires has_int_value
  {
    return ++value_;
  }

  /// Increments the gauge by 1.
  /// @returns The old value of the gauge.
  value_type operator++(int) noexcept
    requires has_int_value
  {
    return value_++;
  }

  /// Decrements the gauge by 1.
  /// @returns The new value of the gauge.
  value_type operator--() noexcept
    requires has_int_value
  {
    return --value_;
  }

  /// Decrements the gauge by 1.
  /// @returns The old value of the gauge.
  value_type operator--(int) noexcept
    requires has_int_value
  {
    return value_--;
  }

  // -- observers --------------------------------------------------------------

  /// Returns the current value of the gauge.
  value_type value() const noexcept {
    return value_.load();
  }

private:
  std::atomic<value_type> value_;
};

/// Convenience alias for a gauge with value type `double`.
using dbl_gauge = gauge<double>;

/// Convenience alias for a gauge with value type `int64_t`.
using int_gauge = gauge<int64_t>;

} // namespace caf::telemetry
