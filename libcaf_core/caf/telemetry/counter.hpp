// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>

#include "caf/config.hpp"
#include "caf/fwd.hpp"
#include "caf/span.hpp"
#include "caf/telemetry/gauge.hpp"
#include "caf/telemetry/label.hpp"

namespace caf::telemetry {

/// A metric that represents a single value that can only go up.
template <class ValueType>
class counter {
public:
  // -- member types -----------------------------------------------------------

  using value_type = ValueType;

  using family_setting = unit_t;

  // -- constants --------------------------------------------------------------

  static constexpr metric_type runtime_type
    = std::is_same<value_type, double>::value ? metric_type::dbl_counter
                                              : metric_type::int_counter;

  // -- constructors, destructors, and assignment operators --------------------

  counter() noexcept = default;

  explicit counter(value_type initial_value) noexcept : gauge_(initial_value) {
    // nop
  }

  explicit counter(span<const label>) noexcept {
    // nop
  }

  // -- modifiers --------------------------------------------------------------

  /// Increments the counter by 1.
  void inc() noexcept {
    gauge_.inc();
  }

  /// Increments the counter by `amount`.
  /// @pre `amount > 0`
  void inc(value_type amount) noexcept {
    CAF_ASSERT(amount > 0);
    gauge_.inc(amount);
  }

  /// Increments the counter by 1.
  /// @returns The new value of the counter.
  template <class T = ValueType>
  std::enable_if_t<std::is_same<T, int64_t>::value, T> operator++() noexcept {
    return ++gauge_;
  }

  // -- observers --------------------------------------------------------------

  /// Returns the current value of the counter.
  value_type value() const noexcept {
    return gauge_.value();
  }

private:
  gauge<value_type> gauge_;
};

/// Convenience alias for a counter with value type `double`.
using dbl_counter = counter<double>;

/// Convenience alias for a counter with value type `int64_t`.
using int_counter = counter<int64_t>;

} // namespace caf::telemetry
