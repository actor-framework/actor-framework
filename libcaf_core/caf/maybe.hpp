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

#ifndef CAF_MAYBE_HPP
#define CAF_MAYBE_HPP

#include <new>
#include <utility>
#include <system_error>

#include "caf/none.hpp"
#include "caf/unit.hpp"
#include "caf/config.hpp"

#include "caf/detail/safe_equal.hpp"

namespace caf {

/// Represents a computation returning either `T` or `std::error_condition`.
/// Note that the error condition might be default-constructed. In this case,
/// a `maybe` represents simply `none`. Hence, this type has three possible
/// states:
/// - A value of `T` is available
///   + `available()` returns `true`
///   + `empty()` returns `false`
///   + `error()` evaluates to `false`
/// -  No value is available but no error occurred
///   + `available()` returns `false`
///   + `empty()` returns `true`
///   + `error()` evaluates to `false`
/// - An error occurred (no value available)
///   + `available()` returns `false`
///   + `empty()` returns `false`
///   + `error()` evaluates to `true`
template <class T>
class maybe {
public:
  using type = typename std::remove_reference<T>::type;
  using reference = type&;
  using const_reference = const type&;
  using pointer = type*;
  using const_pointer = const type*;
  using error_type = std::error_condition;

  /// Type for storing values.
  using storage =
    typename std::conditional<
      std::is_reference<T>::value,
      std::reference_wrapper<type>,
      T
    >::type;

  /// Creates an instance representing an error.
  maybe(error_type err) {
    cr_error(std::move(err));
  }

  /// Creates an instance representing `value`.
  maybe(T value) {
    cr_moved_value(value);
  }

  /// Creates an instance representing an error
  /// from an error condition enum.
  template <class E,
            class = typename std::enable_if<
                      std::is_error_condition_enum<E>::value
                    >::type>
  maybe(E error_code_enum) {
    cr_error(make_error_condition(error_code_enum));
  }

  /// Creates an empty instance.
  maybe() {
    cr_error(error_type{});
  }

  /// Creates an empty instance.
  maybe(const none_t&) {
    cr_error(error_type{});
  }

  maybe(const maybe& other) {
    if (other.available_)
      cr_value(other.value_);
    else
      cr_error(other.error_);
  }

  maybe(maybe&& other) {
    if (other.available_)
      cr_moved_value(other.value_);
    else
      cr_error(std::move(other.error_));
  }

  template <class U>
  maybe(maybe<U>&& other) {
    static_assert(std::is_convertible<U, T>::value, "U not convertible to T");
    if (other)
      cr_moved_value(*other);
    else
      cr_error(std::move(other.error()));
  }

  template <class U>
  maybe(const maybe<U>& other) {
    static_assert(std::is_convertible<U, T>::value, "U not convertible to T");
    if (other)
      cr_value(*other);
    else
      cr_error(other.error());
  }

  ~maybe() {
    destroy();
  }

  maybe& operator=(const none_t&) {
    if (available_) {
      destroy();
      cr_error(error_type{});
    } else if (error_) {
      error_ = error_type{};
    }
    return *this;
  }

  maybe& operator=(T value) {
    if (! available_) {
      destroy();
      cr_moved_value(value);
    } else {
      assign_moved_value(value);
    }
    return *this;
  }

  maybe& operator=(error_type err) {
    if (available_) {
      destroy();
      cr_error(std::move(err));
    } else {
      error_ = std::move(err);
    }
    return *this;
  }

  template <class E,
            class = typename std::enable_if<
              std::is_error_condition_enum<E>::value
            >::type>
  maybe& operator=(E error_code_enum) {
    return *this = make_error_condition(error_code_enum);
  }

  maybe& operator=(maybe&& other) {
    return other ? *this = std::move(*other) : *this = std::move(other.error());
  }

  maybe& operator=(const maybe& other) {
    return other ? *this = *other : *this = other.error();
  }

  template <class U>
  maybe& operator=(maybe<U>&& other) {
    static_assert(std::is_convertible<U, T>::value, "U not convertible to T");
    return other ? *this = std::move(*other) : *this = std::move(other.error());
  }

  template <class U>
  maybe& operator=(const maybe<U>& other) {
    static_assert(std::is_convertible<U, T>::value, "U not convertible to T");
    return other ? *this = *other : *this = other.error();
  }

  /// Queries whether this instance holds a value.
  bool available() const {
    return available_;
  }

  /// Returns `available()`.
  explicit operator bool() const {
    return available();
  }

  /// Returns `! available()`.
  bool operator!() const {
    return ! available();
  }

  /// Returns the value.
  reference get() {
    CAF_ASSERT(available());
    return value_;
  }

  /// Returns the value.
  const_reference get() const {
    CAF_ASSERT(available());
    return value_;
  }

  /// Returns the value.
  reference operator*() {
    return get();
  }

  /// Returns the value.
  const_reference operator*() const {
    return get();
  }

  /// Returns a pointer to the value.
  pointer operator->() {
    return &get();
  }

  /// Returns a pointer to the value.
  const_pointer operator->() const {
    return &get();
  }

  /// Returns whether this objects holds neither a value nor an actual error.
  bool empty() const {
    return ! available() && ! error();
  }

  /// Returns the error.
  const error_type& error() const {
    CAF_ASSERT(! available());
    return error_;
  }

private:
  void destroy() {
    if (available_)
      value_.~storage();
    else
      error_.~error_type();
  }

  void cr_moved_value(reference x) {
    // convert to rvalue if possible
    using fwd_type =
      typename std::conditional<
        ! std::is_reference<T>::value,
        type&&,
        type&
      >::type;
    available_ = true;
    new (&value_) storage(static_cast<fwd_type>(x));
  }

  void assign_moved_value(reference x) {
    using fwd_type =
      typename std::conditional<
        ! std::is_reference<T>::value,
        type&&,
        type&
      >::type;
    value_ = static_cast<fwd_type>(x);
  }

  template <class V>
  void cr_value(V&& x) {
    available_ = true;
    new (&value_) storage(std::forward<V>(x));
  }

  void cr_error(std::error_condition ec) {
    available_ = false;
    new (&error_) error_type(std::move(ec));
  }

  bool available_;
  union {
    storage value_;
    error_type error_;
  };
};

/// Returns `true` if both objects represent either the same
/// value or the same error, `false` otherwise.
/// @relates maybe
template <class T, typename U>
bool operator==(const maybe<T>& lhs, const maybe<U>& rhs) {
  if (lhs)
    return (rhs) ? detail::safe_equal(*lhs, *rhs) : false;
  if (! rhs)
    return lhs.error() == rhs.error();
  return false;
}

/// Returns `true` if `lhs` is available and its value is equal to `rhs`.
template <class T, typename U>
bool operator==(const maybe<T>& lhs, const U& rhs) {
  return (lhs) ? *lhs == rhs : false;
}

/// Returns `true` if `rhs` is available and its value is equal to `lhs`.
/// @relates maybe
template <class T, typename U>
bool operator==(const T& lhs, const maybe<U>& rhs) {
  return rhs == lhs;
}

/// Returns `true` if the objects represent different
/// values or errors, `false` otherwise.
/// @relates maybe
template <class T, typename U>
bool operator!=(const maybe<T>& lhs, const maybe<U>& rhs) {
  return !(lhs == rhs);
}

/// Returns `true` if `lhs` is not available or its value is not equal to `rhs`.
/// @relates maybe
template <class T, typename U>
bool operator!=(const maybe<T>& lhs, const U& rhs) {
  return !(lhs == rhs);
}

/// Returns `true` if `rhs` is not available or its value is not equal to `lhs`.
/// @relates maybe
template <class T, typename U>
bool operator!=(const T& lhs, const maybe<U>& rhs) {
  return !(lhs == rhs);
}

/// Returns `! val.available() && val.error() == err`.
/// @relates maybe
template <class T>
bool operator==(const maybe<T>& val, const std::error_condition& err) {
  return ! val.available() && val.error() == err;
}

/// Returns `! val.available() && val.error() == err`.
/// @relates maybe
template <class T>
bool operator==(const std::error_condition& err, const maybe<T>& val) {
  return val == err;
}

/// Returns `val.available() || val.error() != err`.
/// @relates maybe
template <class T>
bool operator!=(const maybe<T>& val, const std::error_condition& err) {
  return ! (val == err);
}

/// Returns `val.available() || val.error() != err`.
/// @relates maybe
template <class T>
bool operator!=(const std::error_condition& err, const maybe<T>& val) {
  return ! (val == err);
}

/// Returns `val.empty()`.
/// @relates maybe
template <class T>
bool operator==(const maybe<T>& val, const none_t&) {
  return val.empty();
}

/// Returns `val.empty()`.
/// @relates maybe
template <class T>
bool operator==(const none_t&, const maybe<T>& val) {
  return val.empty();
}

/// Returns `! val.empty()`.
/// @relates maybe
template <class T>
bool operator!=(const maybe<T>& val, const none_t&) {
  return ! val.empty();
}

/// Returns `! val.empty()`.
/// @relates maybe
template <class T>
bool operator!=(const none_t&, const maybe<T>& val) {
  return ! val.empty();
}

// Represents a computation performing side effects only and
// optionally return a `std::error_condition`.
template <>
class maybe<void> {
public:
  using type = unit_t;
  using reference = const type&;
  using const_reference = const type&;
  using pointer = const type*;
  using const_pointer = const type*;
  using error_type = std::error_condition;

  maybe() = default;

  maybe(const none_t&) {
    // nop
  }

  maybe(error_type err) : error_(std::move(err)) {
    // nop
  }

  template <class E,
            class = typename std::enable_if<
                      std::is_error_condition_enum<E>::value
                    >::type>
  maybe(E error_code_enum) : error_{error_code_enum} {
    // nop
  }

  maybe& operator=(const none_t&) {
    error_.clear();
    return *this;
  }

  maybe& operator=(error_type err) {
    error_ = std::move(err);
    return *this;
  }

  template <class E,
            class = typename std::enable_if<
                      std::is_error_condition_enum<E>::value
                    >::type>
  maybe& operator=(E error_code_enum) {
    error_ = error_code_enum;
    return *this;
  }

  bool available() const {
    return false;
  }

  explicit operator bool() const {
    return available();
  }

  bool operator!() const {
    return ! available();
  }

  reference get() {
    CAF_ASSERT(! "should never be called");
    return unit;
  }

  const_reference get() const {
    CAF_ASSERT(! "should never be called");
    return unit;
  }

  reference operator*() {
    return get();
  }

  const_reference operator*() const {
    return get();
  }

  pointer operator->() {
    return &get();
  }

  const_pointer operator->() const {
    return &get();
  }

  bool empty() const {
    return ! error();
  }

  const error_type& error() const {
    return error_;
  }

private:
  error_type error_;
};

} // namespace caf

#endif // CAF_MAYBE_HPP
