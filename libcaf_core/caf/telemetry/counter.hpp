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

#include <type_traits>

#include "caf/config.hpp"
#include "caf/fwd.hpp"
#include "caf/telemetry/gauge.hpp"

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

  explicit counter(const std::vector<label>&) noexcept {
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
