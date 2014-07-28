/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_DURATION_HPP
#define CAF_DURATION_HPP

#include <string>
#include <chrono>
#include <cstdint>
#include <stdexcept>

namespace caf {

/**
 * SI time units to specify timeouts.
 */
enum class time_unit : uint32_t {
  invalid = 0,
  seconds = 1,
  milliseconds = 1000,
  microseconds = 1000000

};

// minutes are implicitly converted to seconds
template <intmax_t Num, intmax_t Denom>
struct ratio_to_time_unit_helper {
  static constexpr time_unit value = time_unit::invalid;

};

template <>
struct ratio_to_time_unit_helper<1, 1> {
  static constexpr time_unit value = time_unit::seconds;

};

template <>
struct ratio_to_time_unit_helper<1, 1000> {
  static constexpr time_unit value = time_unit::milliseconds;

};

template <>
struct ratio_to_time_unit_helper<1, 1000000> {
  static constexpr time_unit value = time_unit::microseconds;

};

template <>
struct ratio_to_time_unit_helper<60, 1> {
  static constexpr time_unit value = time_unit::seconds;

};

/**
 * Converts an STL time period to a `time_unit`.
 */
template <class Period>
constexpr time_unit get_time_unit_from_period() {
  return ratio_to_time_unit_helper<Period::num, Period::den>::value;
}

/**
 * Time duration consisting of a `time_unit` and a 64 bit unsigned integer.
 */
class duration {

 public:

  constexpr duration() : unit(time_unit::invalid), count(0) {}

  constexpr duration(time_unit u, uint32_t v) : unit(u), count(v) {}

  /**
   * Creates a new instance from an STL duration.
   * @throws std::invalid_argument Thrown if `d.count() is negative.
   */
  template <class Rep, class Period>
  duration(std::chrono::duration<Rep, Period> d)
      : unit(get_time_unit_from_period<Period>()), count(rd(d)) {
    static_assert(get_time_unit_from_period<Period>() != time_unit::invalid,
            "caf::duration supports only minutes, seconds, "
            "milliseconds or microseconds");
  }

  /**
   * Creates a new instance from an STL duration given in minutes.
   * @throws std::invalid_argument Thrown if `d.count() is negative.
   */
  template <class Rep>
  duration(std::chrono::duration<Rep, std::ratio<60, 1>> d)
      : unit(time_unit::seconds), count(rd(d) * 60) {}

  /**
   * Returns `unit != time_unit::invalid`.
   */
  inline bool valid() const { return unit != time_unit::invalid; }

  /**
   * Returns `count == 0`.
   */
  inline bool is_zero() const { return count == 0; }

  std::string to_string() const;

  time_unit unit;

  uint64_t count;

 private:

  // reads d.count and throws invalid_argument if d.count < 0
  template <class Rep, class Period>
  static inline uint64_t rd(const std::chrono::duration<Rep, Period>& d) {
    if (d.count() < 0) {
      throw std::invalid_argument("negative durations are not supported");
    }
    return static_cast<uint64_t>(d.count());
  }

  template <class Rep>
  static inline uint64_t
  rd(const std::chrono::duration<Rep, std::ratio<60, 1>>& d) {
    // convert minutes to seconds on-the-fly
    return rd(std::chrono::duration<Rep, std::ratio<1, 1>>(d.count()) * 60);
  }

};

/**
 * @relates duration
 */
bool operator==(const duration& lhs, const duration& rhs);

/**
 * @relates duration
 */
inline bool operator!=(const duration& lhs, const duration& rhs) {
  return !(lhs == rhs);
}

} // namespace caf

/**
 * @relates caf::duration
 */
template <class Clock, class Duration>
std::chrono::time_point<Clock, Duration>&
operator+=(std::chrono::time_point<Clock, Duration>& lhs,
       const caf::duration& rhs) {
  switch (rhs.unit) {
    case caf::time_unit::seconds:
      lhs += std::chrono::seconds(rhs.count);
      break;

    case caf::time_unit::milliseconds:
      lhs += std::chrono::milliseconds(rhs.count);
      break;

    case caf::time_unit::microseconds:
      lhs += std::chrono::microseconds(rhs.count);
      break;

    case caf::time_unit::invalid:
      break;
  }
  return lhs;
}

#endif // CAF_DURATION_HPP
