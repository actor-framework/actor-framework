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

namespace caf {
namespace detail {

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

} // namespace details
} // namespace caf

