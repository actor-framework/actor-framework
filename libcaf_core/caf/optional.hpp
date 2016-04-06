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

/// A since C++17 compatible `optional` implementation.
template <class T>
class optional {
 public:
  /// Typdef for `T`.
  using type = T;

  /// Creates an instance without value.
  optional(const none_t& = none) : m_valid(false) {
    // nop
  }

  /// Creates an valid instance from `value`.
  template <class U,
            class E = typename std::enable_if<
                        std::is_convertible<U, T>::value
                      >::type>
  optional(U value) : m_valid(false) {
    cr(std::move(value));
  }

  optional(const optional& other) : m_valid(false) {
    if (other.m_valid) {
      cr(other.m_value);
    }
  }

  optional(optional&& other) : m_valid(false) {
    if (other.m_valid) {
      cr(std::move(other.m_value));
    }
  }

  ~optional() {
    destroy();
  }

  optional& operator=(const optional& other) {
    if (m_valid) {
      if (other.m_valid) m_value = other.m_value;
      else destroy();
    }
    else if (other.m_valid) {
      cr(other.m_value);
    }
    return *this;
  }

  optional& operator=(optional&& other) {
    if (m_valid) {
      if (other.m_valid) m_value = std::move(other.m_value);
      else destroy();
    }
    else if (other.m_valid) {
      cr(std::move(other.m_value));
    }
    return *this;
  }

  /// Checks whether this object contains a value.
  explicit operator bool() const {
    return m_valid;
  }

  /// Checks whether this object does not contain a value.
  bool operator!() const {
    return ! m_valid;
  }

  /// Returns the value.
  T& operator*() {
    CAF_ASSERT(m_valid);
    return m_value;
  }

  /// Returns the value.
  const T& operator*() const {
    CAF_ASSERT(m_valid);
    return m_value;
  }

  /// Returns the value.
  const T* operator->() const {
    CAF_ASSERT(m_valid);
    return &m_value;
  }

  /// Returns the value.
  T* operator->() {
    CAF_ASSERT(m_valid);
    return &m_value;
  }

  /// Returns the value.
  T& value() {
    CAF_ASSERT(m_valid);
    return m_value;
  }

  /// Returns the value.
  const T& value() const {
    CAF_ASSERT(m_valid);
    return m_value;
  }

  /// Returns the value if `m_valid`, otherwise returns `default_value`.
  const T& value_or(const T& default_value) const {
    return m_valid ? value() : default_value;
  }

 private:
  void destroy() {
    if (m_valid) {
      m_value.~T();
      m_valid = false;
    }
  }

  template <class V>
  void cr(V&& value) {
    CAF_ASSERT(! m_valid);
    m_valid = true;
    new (&m_value) T(std::forward<V>(value));
  }

  bool m_valid;
  union { T m_value; };
};

/// Template specialization to allow `optional`
/// to hold a reference rather than an actual value.
template <class T>
class optional<T&> {
 public:
  using type = T;

  optional(const none_t& = none) : m_value(nullptr) {
    // nop
  }

  optional(T& value) : m_value(&value) {
    // nop
  }

  optional(const optional& other) = default;

  optional& operator=(const optional& other) = default;

  explicit operator bool() const {
    return m_value != nullptr;
  }

  bool operator!() const {
    return m_value != nullptr;
  }

  T& operator*() {
    CAF_ASSERT(m_value);
    return *m_value;
  }

  const T& operator*() const {
    CAF_ASSERT(m_value);
    return *m_value;
  }

  T* operator->() {
    CAF_ASSERT(m_value);
    return m_value;
  }

  const T* operator->() const {
    CAF_ASSERT(m_value);
    return m_value;
  }

  T& value() {
    CAF_ASSERT(m_value);
    return *m_value;
  }

  const T& value() const {
    CAF_ASSERT(m_value);
    return *m_value;
  }

  const T& value_or(const T& default_value) const {
    if (m_value)
      return value();
    return default_value;
  }

 private:
  T* m_value;
};

/// @relates optional
template <class T, class U>
bool operator==(const optional<T>& lhs, const optional<U>& rhs) {
  if (lhs)
    return rhs ? detail::safe_equal(*lhs, *rhs) : false;
  return ! rhs;
}

/// @relates optional
template <class T, class U>
bool operator==(const optional<T>& lhs, const U& rhs) {
  return (lhs) ? *lhs == rhs : false;
}

/// @relates optional
template <class T, class U>
bool operator==(const T& lhs, const optional<U>& rhs) {
  return rhs == lhs;
}

/// @relates optional
template <class T, class U>
bool operator!=(const optional<T>& lhs, const optional<U>& rhs) {
  return ! (lhs == rhs);
}

/// @relates optional
template <class T, class U>
bool operator!=(const optional<T>& lhs, const U& rhs) {
  return ! (lhs == rhs);
}

/// @relates optional
template <class T, class U>
bool operator!=(const T& lhs, const optional<U>& rhs) {
  return ! (lhs == rhs);
}

/// @relates optional
template <class T, class U>
bool operator<(const optional<T>& lhs, const optional<U>& rhs) {
  // none is considered smaller than any actual value
  if (lhs)
    return rhs ? *lhs < *rhs : false;
  return static_cast<bool>(rhs);
}

/// @relates optional
template <class T, class U>
bool operator<(const optional<T>& lhs, const U& rhs) {
  if (lhs)
    return *lhs < rhs;
  return true;
}

/// @relates optional
template <class T, class U>
bool operator<(T& lhs, const optional<U>& rhs) {
  if (rhs)
    return lhs < *rhs;
  return false;
}

} // namespace caf

#endif // CAF_OPTIONAL_HPP
