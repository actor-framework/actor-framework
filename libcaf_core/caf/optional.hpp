// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/fwd.hpp"
#include "caf/none.hpp"
#include "caf/unit.hpp"

#include <memory>
#include <new>
#include <utility>

CAF_PUSH_DEPRECATED_WARNING

namespace caf {

/// A C++17 compatible `optional` implementation.
template <class T>
class optional {
public:
  /// Type alias for `T`.
  using type = T;

  using value_type = T;

  /// Creates an instance without value.
  optional(const none_t& = none) : m_valid(false) {
    // nop
  }

  /// Creates an valid instance from `value`.
  template <class U,
            class E
            = typename std::enable_if<std::is_convertible_v<U, T>>::type>
  optional(U x) : m_valid(false) {
    cr(std::move(x));
  }

  optional(const optional& other) : m_valid(false) {
    if (other.m_valid) {
      cr(other.m_value);
    }
  }

  optional(optional&& other) noexcept(
    std::is_nothrow_move_constructible<T>::value)
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
      if (other.m_valid)
        m_value = other.m_value;
      else
        destroy();
    } else if (other.m_valid) {
      cr(other.m_value);
    }
    return *this;
  }

  optional& operator=(optional&& other) noexcept(
    std::is_nothrow_destructible<T>::value&&
      std::is_nothrow_move_assignable<T>::value) {
    if (m_valid) {
      if (other.m_valid)
        m_value = std::move(other.m_value);
      else
        destroy();
    } else if (other.m_valid) {
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

  void reset() {
    destroy();
  }

  template <class... Ts>
  T& emplace(Ts&&... xs) {
    if (m_valid) {
      m_value.~T();
      new (std::addressof(m_value)) T(std::forward<Ts>(xs)...);
    } else {
      new (std::addressof(m_value)) T(std::forward<Ts>(xs)...);
      m_valid = true;
    }
    return m_value;
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
  union {
    T m_value;
  };
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

/// @relates optional
template <class T>
auto to_string(const optional<T>& x)
  -> decltype(to_string(std::declval<const T&>())) {
  return x ? "*" + to_string(*x) : "null";
}

/// Returns an rvalue to the value managed by `x`.
/// @relates optional
template <class T>
T&& move_if_optional(optional<T>& x) {
  return std::move(*x);
}

/// Returns `*x`.
/// @relates optional
template <class T>
T& move_if_optional(T* x) {
  return *x;
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

CAF_POP_WARNINGS
