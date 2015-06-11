/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_OPTIONAL_HPP
#define CAF_OPTIONAL_HPP

#include <new>
#include <utility>

#include "caf/none.hpp"
#include "caf/unit.hpp"
#include "caf/config.hpp"

#include "caf/detail/safe_equal.hpp"

namespace caf {

/// Represents an optional value of `T`.
template <class T>
class optional {
public:
  /// Typdef for `T`.
  using type = T;

  /// Creates an empty instance with `valid() == false`.
  optional(const none_t& = none) : valid_(false) {
    // nop
  }

  /// Creates an valid instance from `value`.
  optional(T value) : valid_(false) {
    cr(std::move(value));
  }

  optional(const optional& other) : valid_(false) {
    if (other.valid_) {
      cr(other.value_);
    }
  }

  optional(optional&& other) : valid_(false) {
    if (other.valid_) {
      cr(std::move(other.value_));
    }
  }

  ~optional() {
    destroy();
  }

  optional& operator=(const optional& other) {
    if (valid_) {
      if (other.valid_) value_ = other.value_;
      else destroy();
    }
    else if (other.valid_) {
      cr(other.value_);
    }
    return *this;
  }

  optional& operator=(optional&& other) {
    if (valid_) {
      if (other.valid_) value_ = std::move(other.value_);
      else destroy();
    }
    else if (other.valid_) {
      cr(std::move(other.value_));
    }
    return *this;
  }

  /// Queries whether this instance holds a value.
  bool valid() const {
    return valid_;
  }

  /// Returns `!valid()`.
  bool empty() const {
    return ! valid_;
  }

  /// Returns `valid()`.
  explicit operator bool() const {
    return valid();
  }

  /// Returns `!valid()`.
  bool operator!() const {
    return ! valid();
  }

  /// Returns the value.
  T& operator*() {
    CAF_ASSERT(valid());
    return value_;
  }

  /// Returns the value.
  const T& operator*() const {
    CAF_ASSERT(valid());
    return value_;
  }

  /// Returns the value.
  const T* operator->() const {
    CAF_ASSERT(valid());
    return &value_;
  }

  /// Returns the value.
  T* operator->() {
    CAF_ASSERT(valid());
    return &value_;
  }

  /// Returns the value.
  T& get() {
    CAF_ASSERT(valid());
    return value_;
  }

  /// Returns the value.
  const T& get() const {
    CAF_ASSERT(valid());
    return value_;
  }

  /// Returns the value if `valid()`, otherwise returns `default_value`.
  const T& get_or_else(const T& default_value) const {
    return valid() ? get() : default_value;
  }

private:
  void destroy() {
    if (valid_) {
      value_.~T();
      valid_ = false;
    }
  }
  template <class V>
  void cr(V&& value) {
    CAF_ASSERT(! valid());
    valid_ = true;
    new (&value_) T(std::forward<V>(value));
  }
  bool valid_;
  union { T value_; };
};

/// Template specialization to allow `optional`
/// to hold a reference rather than an actual value.
template <class T>
class optional<T&> {
public:
  using type = T;

  optional(const none_t& = none) : value_(nullptr) {
    // nop
  }

  optional(T& value) : value_(&value) {
    // nop
  }

  optional(const optional& other) = default;

  optional& operator=(const optional& other) = default;

  bool valid() const {
    return value_ != nullptr;
  }

  bool empty() const {
    return ! valid();
  }

  explicit operator bool() const {
    return valid();
  }

  bool operator!() const {
    return ! valid();
  }

  T& operator*() {
    CAF_ASSERT(valid());
    return *value_;
  }

  const T& operator*() const {
    CAF_ASSERT(valid());
    return *value_;
  }

  T* operator->() {
    CAF_ASSERT(valid());
    return value_;
  }

  const T* operator->() const {
    CAF_ASSERT(valid());
    return value_;
  }

  T& get() {
    CAF_ASSERT(valid());
    return *value_;
  }

  const T& get() const {
    CAF_ASSERT(valid());
    return *value_;
  }

  const T& get_or_else(const T& default_value) const {
    if (valid()) return get();
    return default_value;
  }

private:
  T* value_;
};

/// @relates optional
template <class T, typename U>
bool operator==(const optional<T>& lhs, const optional<U>& rhs) {
  if ((lhs) && (rhs)) {
    return detail::safe_equal(*lhs, *rhs);
  }
  return ! lhs && ! rhs;
}

/// @relates optional
template <class T, typename U>
bool operator==(const optional<T>& lhs, const U& rhs) {
  return (lhs) ? *lhs == rhs : false;
}

/// @relates optional
template <class T, typename U>
bool operator==(const T& lhs, const optional<U>& rhs) {
  return rhs == lhs;
}

/// @relates optional
template <class T, typename U>
bool operator!=(const optional<T>& lhs, const optional<U>& rhs) {
  return !(lhs == rhs);
}

/// @relates optional
template <class T, typename U>
bool operator!=(const optional<T>& lhs, const U& rhs) {
  return !(lhs == rhs);
}

/// @relates optional
template <class T, typename U>
bool operator!=(const T& lhs, const optional<U>& rhs) {
  return !(lhs == rhs);
}

/// @relates optional
template <class T>
bool operator==(const optional<T>& val, const none_t&) {
  return ! val.valid();
}

/// @relates optional
template <class T>
bool operator==(const none_t&, const optional<T>& val) {
  return ! val.valid();
}
/// @relates optional
template <class T>
bool operator!=(const optional<T>& lhs, const none_t& rhs) {
  return !(lhs == rhs);
}

/// @relates optional
template <class T>
bool operator!=(const none_t& lhs, const optional<T>& rhs) {
  return !(lhs == rhs);
}

} // namespace caf

#endif // CAF_OPTIONAL_HPP
