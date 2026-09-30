// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <functional>

namespace caf::detail {

/// Three-way comparison using std::less for implementing `compare()` functions.
/// @returns -1 if `lhs < rhs`, 0 if `lhs == rhs`, and 1 if `lhs > rhs`.
template <class T, class U>
constexpr int three_way_compare(T&& lhs, U&& rhs) noexcept {
  using lhs_t = std::decay_t<T>;
  using rhs_t = std::decay_t<U>;
  static_assert(!std::is_floating_point_v<lhs_t>
                  && !std::is_floating_point_v<rhs_t>,
                "this algorithm is unsafe for floating points (due to NaN)");
  std::less<void> is_less;
  if (is_less(lhs, rhs)) {
    return -1;
  }
  if (is_less(rhs, lhs)) {
    return 1;
  }
  return 0;
}

/// Barton–Nackman trick implementation.
/// `Subclass` must provide a compare member function that compares
/// to instances of `T` and returns an integer x with:
/// - `x < 0` if `*this < other`
/// - `x > 0` if `*this > other`
/// - `x == 0` if `*this == other`
template <class Subclass, class T = Subclass>
class comparable {
  friend bool operator==(const Subclass& lhs, const T& rhs) noexcept {
    return lhs.compare(rhs) == 0;
  }

  friend bool operator==(const T& lhs, const Subclass& rhs) noexcept {
    return rhs.compare(lhs) == 0;
  }

  friend bool operator!=(const Subclass& lhs, const T& rhs) noexcept {
    return lhs.compare(rhs) != 0;
  }

  friend bool operator!=(const T& lhs, const Subclass& rhs) noexcept {
    return rhs.compare(lhs) != 0;
  }

  friend bool operator<(const Subclass& lhs, const T& rhs) noexcept {
    return lhs.compare(rhs) < 0;
  }

  friend bool operator>(const Subclass& lhs, const T& rhs) noexcept {
    return lhs.compare(rhs) > 0;
  }

  friend bool operator<(const T& lhs, const Subclass& rhs) noexcept {
    return rhs > lhs;
  }

  friend bool operator>(const T& lhs, const Subclass& rhs) noexcept {
    return rhs < lhs;
  }

  friend bool operator<=(const Subclass& lhs, const T& rhs) noexcept {
    return lhs.compare(rhs) <= 0;
  }

  friend bool operator>=(const Subclass& lhs, const T& rhs) noexcept {
    return lhs.compare(rhs) >= 0;
  }

  friend bool operator<=(const T& lhs, const Subclass& rhs) noexcept {
    return rhs >= lhs;
  }

  friend bool operator>=(const T& lhs, const Subclass& rhs) noexcept {
    return rhs <= lhs;
  }
};

template <class Subclass>
class comparable<Subclass, Subclass> {
  friend bool operator==(const Subclass& lhs, const Subclass& rhs) noexcept {
    return lhs.compare(rhs) == 0;
  }

  friend bool operator!=(const Subclass& lhs, const Subclass& rhs) noexcept {
    return lhs.compare(rhs) != 0;
  }

  friend bool operator<(const Subclass& lhs, const Subclass& rhs) noexcept {
    return lhs.compare(rhs) < 0;
  }

  friend bool operator<=(const Subclass& lhs, const Subclass& rhs) noexcept {
    return lhs.compare(rhs) <= 0;
  }

  friend bool operator>(const Subclass& lhs, const Subclass& rhs) noexcept {
    return lhs.compare(rhs) > 0;
  }

  friend bool operator>=(const Subclass& lhs, const Subclass& rhs) noexcept {
    return lhs.compare(rhs) >= 0;
  }
};

} // namespace caf::detail
