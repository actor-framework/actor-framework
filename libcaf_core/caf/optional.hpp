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

#ifndef CAF_OPTIONAL_HPP
#define CAF_OPTIONAL_HPP

#include <new>
#include <utility>

#include "caf/none.hpp"
#include "caf/unit.hpp"
#include "caf/config.hpp"

namespace caf {

/**
 * @brief Represents an optional value of @p T.
 */
template <class T>
class optional {

 public:

  /**
   * @brief Typdef for @p T.
   */
  using type = T;

  /* *
   * @brief Default constructor.
   * @post <tt>valid() == false</tt>
   */
  //optional() : m_valid(false) { }

  /**
   * @post <tt>valid() == false</tt>
   */
  optional(const none_t& = none) : m_valid(false) { }

  /**
   * @brief Creates an @p option from @p value.
   * @post <tt>valid() == true</tt>
   */
  optional(T value) : m_valid(false) { cr(std::move(value)); }

  optional(const optional& other) : m_valid(false) {
    if (other.m_valid) cr(other.m_value);
  }

  optional(optional&& other) : m_valid(false) {
    if (other.m_valid) cr(std::move(other.m_value));
  }

  ~optional() { destroy(); }

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
   * @brief Returns @p true if this @p option has a valid value;
   *    otherwise @p false.
   */
  inline bool valid() const { return m_valid; }

  /**
   * @brief Returns <tt>!valid()</tt>.
   */
  inline bool empty() const { return !m_valid; }

  /**
   * @brief Returns @p true if this @p option has a valid value;
   *    otherwise @p false.
   */
  inline explicit operator bool() const { return valid(); }

  /**
   * @brief Returns <tt>!valid()</tt>.
   */
  inline bool operator!() const { return empty(); }

  inline bool operator==(const none_t&) { return empty(); }

  /**
   * @brief Returns the value.
   */
  inline T& operator*() {
    CAF_REQUIRE(valid());
    return m_value;
  }

  /**
   * @brief Returns the value.
   */
  inline const T& operator*() const {
    CAF_REQUIRE(valid());
    return m_value;
  }

  /**
   * @brief Returns the value.
   */
  inline const T* operator->() const {
    CAF_REQUIRE(valid());
    return &m_value;
  }

  /**
   * @brief Returns the value.
   */
  inline T* operator->() {
    CAF_REQUIRE(valid());
    return &m_value;
  }

  /**
   * @brief Returns the value.
   */
  inline T& get() {
    CAF_REQUIRE(valid());
    return m_value;
  }

  /**
   * @brief Returns the value.
   */
  inline const T& get() const {
    CAF_REQUIRE(valid());
    return m_value;
  }

  /**
   * @brief Returns the value. The value is set to @p default_value
   *    if <tt>valid() == false</tt>.
   * @post <tt>valid() == true</tt>
   */
  inline const T& get_or_else(const T& default_value) const {
    if (valid()) return get();
    return default_value;
  }

 private:

  bool m_valid;
  union { T m_value; };

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
    new (&m_value) T (std::forward<V>(value));
  }

};

template <class T>
class optional<T&> {

 public:

  using type = T;

  optional(const none_t& = none) : m_value(nullptr) { }

  optional(T& value) : m_value(&value) { }

  optional(const optional& other) = default;

  optional& operator=(const optional& other) = default;

  inline bool valid() const { return m_value != nullptr; }

  inline bool empty() const { return !valid(); }

  inline explicit operator bool() const { return valid(); }

  inline bool operator!() const { return empty(); }

  inline bool operator==(const none_t&) { return empty(); }

  inline T& operator*() {
    CAF_REQUIRE(valid());
    return *m_value;
  }

  inline const T& operator*() const {
    CAF_REQUIRE(valid());
    return *m_value;
  }

  inline T* operator->() {
    CAF_REQUIRE(valid());
    return m_value;
  }

  inline const T* operator->() const {
    CAF_REQUIRE(valid());
    return m_value;
  }

  inline T& get() {
    CAF_REQUIRE(valid());
    return *m_value;
  }

  inline const T& get() const {
    CAF_REQUIRE(valid());
    return *m_value;
  }

  inline const T& get_or_else(const T& default_value) const {
    if (valid()) return get();
    return default_value;
  }

 private:

  T* m_value;

};

template <>
class optional<void> {

 public:

  optional(const unit_t&) : m_valid(true) { }

  optional(const none_t& = none) : m_valid(false) { }

  inline bool valid() const { return m_valid; }

  inline bool empty() const { return !m_valid; }

  inline explicit operator bool() const { return valid(); }

  inline bool operator!() const { return empty(); }

  inline bool operator==(const none_t&) { return empty(); }

  inline const unit_t& operator*() const { return unit; }

 private:

  bool m_valid;

};

/** @relates option */
template <class T, typename U>
bool operator==(const optional<T>& lhs, const optional<U>& rhs) {
  if ((lhs) && (rhs)) return *lhs == *rhs;
  return false;
}

/** @relates option */
template <class T, typename U>
bool operator==(const optional<T>& lhs, const U& rhs) {
  if (lhs) return *lhs == rhs;
  return false;
}

/** @relates option */
template <class T, typename U>
bool operator==(const T& lhs, const optional<U>& rhs) {
  return rhs == lhs;
}

/** @relates option */
template <class T, typename U>
bool operator!=(const optional<T>& lhs, const optional<U>& rhs) {
  return !(lhs == rhs);
}

/** @relates option */
template <class T, typename U>
bool operator!=(const optional<T>& lhs, const U& rhs) {
  return !(lhs == rhs);
}

/** @relates option */
template <class T, typename U>
bool operator!=(const T& lhs, const optional<U>& rhs) {
  return !(lhs == rhs);
}

} // namespace caf

#endif // CAF_OPTIONAL_HPP
