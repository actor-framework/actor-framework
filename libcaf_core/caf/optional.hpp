/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#include <memory>
#include <new>
#include <utility>

#include "caf/none.hpp"
#include "caf/unit.hpp"
#include "caf/config.hpp"
#include "caf/deep_to_string.hpp"

#include "caf/detail/safe_equal.hpp"
#include "caf/detail/scope_guard.hpp"

namespace caf {

/// A C++17 compatible `optional` implementation.
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
  optional(U x) : m_valid(false) {
    cr(std::move(x));
  }

  optional(const optional& other) : m_valid(false) {
    if (other.m_valid) {
      cr(other.m_value);
    }
  }

  optional(optional&& other)
  noexcept(std::is_nothrow_move_constructible<T>::value)
    : m_valid(false) {
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

  optional& operator=(optional&& other)
  noexcept(std::is_nothrow_destructible<T>::value &&
           std::is_nothrow_move_assignable<T>::value) {
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
    return !m_valid;
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
  void cr(V&& x) {
    CAF_ASSERT(!m_valid);
    m_valid = true;
    new (std::addressof(m_value)) T(std::forward<V>(x));
  }

  bool m_valid;
  union { T m_value; };
};

/// Template specialization to allow `optional` to hold a reference
/// rather than an actual value with minimal overhead.
template <class T>
class optional<T&> {
 public:
  using type = T;

  optional(const none_t& = none) : m_value(nullptr) {
    // nop
  }

  optional(T& x) : m_value(&x) {
    // nop
  }

  optional(T* x) : m_value(x) {
    // nop
  }

  optional(const optional& other) = default;

  optional& operator=(const optional& other) = default;

  explicit operator bool() const {
    return m_value != nullptr;
  }

  bool operator!() const {
    return !m_value;
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

template <>
class optional<void> {
 public:
  using type = unit_t;

  optional(none_t = none) : m_value(false) {
    // nop
  }

  optional(unit_t) : m_value(true) {
    // nop
  }

  optional(const optional& other) = default;

  optional& operator=(const optional& other) = default;

  explicit operator bool() const {
    return m_value;
  }

  bool operator!() const {
    return !m_value;
  }

 private:
  bool m_value;
};

template <class Inspector, class T>
typename std::enable_if<Inspector::reads_state,
                        typename Inspector::result_type>::type
inspect(Inspector& f, optional<T>& x) {
  return x ? f(true, *x) : f(false);
}

template <class T>
struct optional_inspect_helper {
  bool& enabled;
  T& storage;
  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f,
                                                 optional_inspect_helper& x) {
    return x.enabled ? f(x.storage) : f();
  }
};

template <class Inspector, class T>
typename std::enable_if<Inspector::writes_state,
                        typename Inspector::result_type>::type
inspect(Inspector& f, optional<T>& x) {
  bool flag = false;
  typename optional<T>::type tmp{};
  optional_inspect_helper<T> helper{flag, tmp};
  auto guard = detail::make_scope_guard([&] {
    if (flag)
      x = std::move(tmp);
    else
      x = none;
  });
  return f(flag, helper);
}

/// @relates optional
template <class T>
std::string to_string(const optional<T>& x) {
  return x ? "*" + deep_to_string(*x) : "none";
}

// -- [X.Y.8] comparison with optional ----------------------------------------

/// @relates optional
template <class T>
bool operator==(const optional<T>& lhs, const optional<T>& rhs) {
  return static_cast<bool>(lhs) == static_cast<bool>(rhs)
      && (!lhs || *lhs == *rhs);
}

/// @relates optional
template <class T>
bool operator!=(const optional<T>& lhs, const optional<T>& rhs) {
  return !(lhs == rhs);
}

/// @relates optional
template <class T>
bool operator<(const optional<T>& lhs, const optional<T>& rhs) {
  return static_cast<bool>(rhs) && (!lhs || *lhs < *rhs);
}

/// @relates optional
template <class T>
bool operator<=(const optional<T>& lhs, const optional<T>& rhs) {
  return !(rhs < lhs);
}

/// @relates optional
template <class T>
bool operator>=(const optional<T>& lhs, const optional<T>& rhs) {
  return !(lhs < rhs);
}

/// @relates optional
template <class T>
bool operator>(const optional<T>& lhs, const optional<T>& rhs) {
  return rhs < lhs;
}

// -- [X.Y.9] comparison with none_t (aka. nullopt_t) -------------------------

/// @relates optional
template <class T>
bool operator==(const optional<T>& lhs, none_t) {
  return !lhs;
}

/// @relates optional
template <class T>
bool operator==(none_t, const optional<T>& rhs) {
  return !rhs;
}

/// @relates optional
template <class T>
bool operator!=(const optional<T>& lhs, none_t) {
  return static_cast<bool>(lhs);
}

/// @relates optional
template <class T>
bool operator!=(none_t, const optional<T>& rhs) {
  return static_cast<bool>(rhs);
}

/// @relates optional
template <class T>
bool operator<(const optional<T>&, none_t) {
  return false;
}

/// @relates optional
template <class T>
bool operator<(none_t, const optional<T>& rhs) {
  return static_cast<bool>(rhs);
}

/// @relates optional
template <class T>
bool operator<=(const optional<T>& lhs, none_t) {
  return !lhs;
}

/// @relates optional
template <class T>
bool operator<=(none_t, const optional<T>&) {
  return true;
}

/// @relates optional
template <class T>
bool operator>(const optional<T>& lhs, none_t) {
  return static_cast<bool>(lhs);
}

/// @relates optional
template <class T>
bool operator>(none_t, const optional<T>&) {
  return false;
}

/// @relates optional
template <class T>
bool operator>=(const optional<T>&, none_t) {
  return true;
}

/// @relates optional
template <class T>
bool operator>=(none_t, const optional<T>&) {
  return true;
}

// -- [X.Y.10] comparison with value type ------------------------------------

/// @relates optional
template <class T>
bool operator==(const optional<T>& lhs, const T& rhs) {
  return lhs && *lhs == rhs;
}

/// @relates optional
template <class T>
bool operator==(const T& lhs, const optional<T>& rhs) {
  return rhs && lhs == *rhs;
}

/// @relates optional
template <class T>
bool operator!=(const optional<T>& lhs, const T& rhs) {
  return !lhs || !(*lhs == rhs);
}

/// @relates optional
template <class T>
bool operator!=(const T& lhs, const optional<T>& rhs) {
  return !rhs || !(lhs == *rhs);
}

/// @relates optional
template <class T>
bool operator<(const optional<T>& lhs, const T& rhs) {
  return !lhs || *lhs < rhs;
}

/// @relates optional
template <class T>
bool operator<(const T& lhs, const optional<T>& rhs) {
  return rhs && lhs < *rhs;
}

/// @relates optional
template <class T>
bool operator<=(const optional<T>& lhs, const T& rhs) {
  return !lhs || !(rhs < *lhs);
}

/// @relates optional
template <class T>
bool operator<=(const T& lhs, const optional<T>& rhs) {
  return rhs && !(rhs < lhs);
}

/// @relates optional
template <class T>
bool operator>(const optional<T>& lhs, const T& rhs) {
  return lhs && rhs < *lhs;
}

/// @relates optional
template <class T>
bool operator>(const T& lhs, const optional<T>& rhs) {
  return !rhs || *rhs < lhs;
}

/// @relates optional
template <class T>
bool operator>=(const optional<T>& lhs, const T& rhs) {
  return lhs && !(*lhs < rhs);
}

/// @relates optional
template <class T>
bool operator>=(const T& lhs, const optional<T>& rhs) {
  return !rhs || !(lhs < *rhs);
}

} // namespace caf

