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

namespace caf {

/**
 * Represents an optional value of `T`.
 */
template <class T>
class optional {
 public:
  /**
   * Typdef for `T`.
   */
  using type = T;

  /**
   * Creates an empty instance with `valid() == false`.
   */
  optional(const none_t& = none) : m_valid(false) {
    // nop
  }

  /**
   * Creates an valid instance from `value`.
   */
  optional(T value) : m_valid(false) {
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

  /**
   * Queries whether this instance holds a value.
   */
  bool valid() const {
    return m_valid;
  }

  /**
   * Returns `!valid()`.
   */
  bool empty() const {
    return !m_valid;
  }

  /**
   * Returns `valid()`.
   */
  explicit operator bool() const {
    return valid();
  }

  /**
   * Returns `!valid()`.
   */
  bool operator!() const {
    return !valid();
  }

  /**
   * Returns the value.
   */
  T& operator*() {
    CAF_REQUIRE(valid());
    return m_value;
  }

  /**
   * Returns the value.
   */
  const T& operator*() const {
    CAF_REQUIRE(valid());
    return m_value;
  }

  /**
   * Returns the value.
   */
  const T* operator->() const {
    CAF_REQUIRE(valid());
    return &m_value;
  }

  /**
   * Returns the value.
   */
  T* operator->() {
    CAF_REQUIRE(valid());
    return &m_value;
  }

  /**
   * Returns the value.
   */
  T& get() {
    CAF_REQUIRE(valid());
    return m_value;
  }

  /**
   * Returns the value.
   */
  const T& get() const {
    CAF_REQUIRE(valid());
    return m_value;
  }

  /**
   * Returns the value if `valid()`, otherwise returns `default_value`.
   */
  const T& get_or_else(const T& default_value) const {
    return valid() ? get() : default_value;
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
    CAF_REQUIRE(!valid());
    m_valid = true;
    new (&m_value) T(std::forward<V>(value));
  }
  bool m_valid;
  union { T m_value; };
};

/**
 * Template specialization to allow `optional`
 * to hold a reference rather than an actual value.
 */
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

  bool valid() const {
    return m_value != nullptr;
  }

  bool empty() const {
    return !valid();
  }

  explicit operator bool() const {
    return valid();
  }

  bool operator!() const {
    return !valid();
  }

  T& operator*() {
    CAF_REQUIRE(valid());
    return *m_value;
  }

  const T& operator*() const {
    CAF_REQUIRE(valid());
    return *m_value;
  }

  T* operator->() {
    CAF_REQUIRE(valid());
    return m_value;
  }

  const T* operator->() const {
    CAF_REQUIRE(valid());
    return m_value;
  }

  T& get() {
    CAF_REQUIRE(valid());
    return *m_value;
  }

  const T& get() const {
    CAF_REQUIRE(valid());
    return *m_value;
  }

  const T& get_or_else(const T& default_value) const {
    if (valid()) return get();
    return default_value;
  }

 private:
  T* m_value;
};

/** @relates optional */
template <class T, typename U>
bool operator==(const optional<T>& lhs, const optional<U>& rhs) {
  if ((lhs) && (rhs)) {
    return *lhs == *rhs;
  }
  return !lhs && !rhs;
}

/** @relates optional */
template <class T, typename U>
bool operator==(const optional<T>& lhs, const U& rhs) {
  return (lhs) ? *lhs == rhs : false;
}

/** @relates optional */
template <class T, typename U>
bool operator==(const T& lhs, const optional<U>& rhs) {
  return rhs == lhs;
}

/** @relates optional */
template <class T, typename U>
bool operator!=(const optional<T>& lhs, const optional<U>& rhs) {
  return !(lhs == rhs);
}

/** @relates optional */
template <class T, typename U>
bool operator!=(const optional<T>& lhs, const U& rhs) {
  return !(lhs == rhs);
}

/** @relates optional */
template <class T, typename U>
bool operator!=(const T& lhs, const optional<U>& rhs) {
  return !(lhs == rhs);
}

/** @relates optional */
template <class T>
bool operator==(const optional<T>& val, const none_t&) {
  return val.valid();
}

/** @relates optional */
template <class T>
bool operator==(const none_t&, const optional<T>& val) {
  return val.valid();
}
/** @relates optional */
template <class T>
bool operator!=(const optional<T>& val, const none_t&) {
  return !val.valid();
}

/** @relates optional */
template <class T>
bool operator!=(const none_t&, const optional<T>& val) {
  return !val.valid();
}

} // namespace caf

#endif // CAF_OPTIONAL_HPP
