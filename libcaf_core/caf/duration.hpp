/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#include <string>
#include <chrono>
#include <cstdint>
#include <stdexcept>

#include "caf/error.hpp"

namespace caf {

/// SI time units to specify timeouts.
/// @relates duration
enum class time_unit : uint32_t {
  invalid,
  minutes,
  seconds,
  milliseconds,
  microseconds,
  nanoseconds
};

/// Relates time_unit
std::string to_string(time_unit x);

// Calculates the index of a time_unit from the denominator of a ratio.
constexpr intmax_t denom_to_unit_index(intmax_t x, intmax_t offset = 2) {
  return x < 1000 ? (x == 1 ? offset : 0)
                  : denom_to_unit_index(x / 1000, offset + 1);
}

constexpr time_unit denom_to_time_unit(intmax_t x) {
  return static_cast<time_unit>(denom_to_unit_index(x));
}

/// Converts the ratio Num/Denom to a `time_unit` if the ratio describes
/// seconds, milliseconds, microseconds, or minutes. Minutes are mapped
/// to `time_unit::seconds`, any unrecognized ratio to `time_unit::invalid`.
/// @relates duration
template <intmax_t Num, intmax_t Denom>
struct ratio_to_time_unit_helper;

template <intmax_t Denom>
struct ratio_to_time_unit_helper<1, Denom> {
  static constexpr time_unit value = denom_to_time_unit(Denom);
};

template <>
struct ratio_to_time_unit_helper<60, 1> {
  static constexpr time_unit value = time_unit::minutes;
};

/// Converts an STL time period to a `time_unit`.
/// @relates duration
template <class Period>
constexpr time_unit get_time_unit_from_period() {
  return ratio_to_time_unit_helper<Period::num, Period::den>::value;
}

/// Represents an infinite amount of timeout for specifying "invalid" timeouts.
struct infinite_t {
  constexpr infinite_t() {
    // nop
  }
};

static constexpr infinite_t infinite = infinite_t{};

/// Time duration consisting of a `time_unit` and a 64 bit unsigned integer.
class duration {
public:
  constexpr duration() : unit(time_unit::invalid), count(0) {
    // nop
  }

  constexpr duration(time_unit u, uint32_t v) : unit(u), count(v) {
    // nop
  }

  constexpr duration(infinite_t) : unit(time_unit::invalid), count(0) {
    // nop
  }

  /// Creates a new instance from an STL duration.
  /// @throws std::invalid_argument Thrown if `d.count() is negative.
  template <class Rep, class Period,
            class E =
              typename std::enable_if<
                std::is_integral<Rep>::value
                && get_time_unit_from_period<Period>() != time_unit::invalid
              >::type>
  explicit duration(const std::chrono::duration<Rep, Period>& d)
      : unit(get_time_unit_from_period<Period>()),
        count(d.count() < 0 ? 0u : static_cast<uint64_t>(d.count())) {
    // nop
  }

  /// Returns `unit != time_unit::invalid`.
  constexpr bool valid() const {
    return unit != time_unit::invalid;
  }

  /// Returns `count == 0`.
  constexpr bool is_zero() const {
    return count == 0;
  }

  time_unit unit;

  uint64_t count;
};

/// @relates duration
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, duration& x) {
  return f(x.unit, x.count);
}

/// @relates duration
std::string to_string(const duration& x);

/// @relates duration
bool operator==(const duration& lhs, const duration& rhs);

/// @relates duration
inline bool operator!=(const duration& lhs, const duration& rhs) {
  return !(lhs == rhs);
}

/// @relates duration
template <class Clock, class Duration>
std::chrono::time_point<Clock, Duration>&
operator+=(std::chrono::time_point<Clock, Duration>& lhs, const duration& rhs) {
  switch (rhs.unit) {
    case time_unit::invalid:
      break;
    case time_unit::minutes:
      lhs += std::chrono::minutes(rhs.count);
      break;
    case time_unit::seconds:
      lhs += std::chrono::seconds(rhs.count);
      break;
    case time_unit::milliseconds:
      lhs += std::chrono::milliseconds(rhs.count);
      break;
    case time_unit::microseconds:
      lhs += std::chrono::microseconds(rhs.count);
      break;
    case time_unit::nanoseconds:
      lhs += std::chrono::nanoseconds(rhs.count);
      break;
  }
  return lhs;
}

} // namespace caf

