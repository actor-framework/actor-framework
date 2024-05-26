// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/span.hpp"
#include "caf/telemetry/label.hpp"
#include "caf/telemetry/metric_type.hpp"

#include <atomic>
#include <cstdint>

namespace caf::telemetry {

/// A metric that represents a single integer value that can arbitrarily go up
/// and down.
class CAF_CORE_EXPORT int_gauge {
public:
  // -- member types -----------------------------------------------------------

  using value_type = int64_t;

  using family_setting = unit_t;

  // -- constants --------------------------------------------------------------

  static constexpr metric_type runtime_type = metric_type::int_gauge;

  // -- constructors, destructors, and assignment operators --------------------

  int_gauge() noexcept : value_(0) {
    // nop
  }

  explicit int_gauge(int64_t value) noexcept : value_(value) {
    // nop
  }

  explicit int_gauge(span<const label>) noexcept : value_(0) {
    // nop
  }

  // -- modifiers --------------------------------------------------------------

  /// Increments the gauge by 1.
  void inc() noexcept {
    ++value_;
  }

  /// Increments the gauge by `amount`.
  void inc(int64_t amount) noexcept {
    value_.fetch_add(amount);
  }

  /// Decrements the gauge by 1.
  void dec() noexcept {
    --value_;
  }

  /// Decrements the gauge by `amount`.
  void dec(int64_t amount) noexcept {
    value_.fetch_sub(amount);
  }

  /// Sets the gauge to `x`.
  void value(int64_t x) noexcept {
    value_.store(x);
  }

  /// Increments the gauge by 1.
  /// @returns The new value of the gauge.
  int64_t operator++() noexcept {
    return ++value_;
  }

  /// Increments the gauge by 1.
  /// @returns The old value of the gauge.
  int64_t operator++(int) noexcept {
    return value_++;
  }

  /// Decrements the gauge by 1.
  /// @returns The new value of the gauge.
  int64_t operator--() noexcept {
    return --value_;
  }

  /// Decrements the gauge by 1.
  /// @returns The old value of the gauge.
  int64_t operator--(int) noexcept {
    return value_--;
  }

  // -- observers --------------------------------------------------------------

  /// Returns the current value of the gauge.
  int64_t value() const noexcept {
    return value_.load();
  }

private:
  std::atomic<int64_t> value_;
};

} // namespace caf::telemetry
