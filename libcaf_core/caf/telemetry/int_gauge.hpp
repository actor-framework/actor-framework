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

#include "caf/fwd.hpp"
#include "caf/span.hpp"
#include "caf/telemetry/label.hpp"
#include "caf/telemetry/metric_type.hpp"

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

  /// Decrements the gauge by 1.
  /// @returns The new value of the gauge.
  int64_t operator--() noexcept {
    return --value_;
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
