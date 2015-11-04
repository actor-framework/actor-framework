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
/// - Enganged (holds a `T`)
///   + `valid() == true`
///   + `is_none() == false`
///   + `has_error() == false`
/// - Not engaged without actual error (default-constructed `error_condition`)
///   + `valid() == false`
///   + `is_none() == true`
///   + `has_error() == false`
/// - Not engaged with error
///   + `valid() == false`
///   + `is_none() == false`
///   + `has_error() == true`
template <class T>
class maybe {
public:
  using type = typename std::remove_reference<T>::type;
  using reference = type&;
  using const_reference = const type&;
  using pointer = type*;
  using const_pointer = const type*;
  using error_type = std::error_condition;

  /// The type used for storing values.
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

  /// Creates an empty instance.
  maybe() {
    cr_error(error_type{});
  }

  /// Creates an empty instance.
  maybe(const none_t&) {
    cr_error(error_type{});
  }

  maybe(const maybe& other) {
    if (other.valid_)
      cr_value(other.value_);
    else
      cr_error(other.error_);
  }

  maybe(maybe&& other) : valid_(other.valid_) {
    if (other.valid_)
      cr_moved_value(other.value_);
    else
      cr_error(std::move(other.error_));
  }

  ~maybe() {
    destroy();
  }

  maybe& operator=(const none_t&) {
    if (valid_) {
      destroy();
      cr_error(error_type{});
    } else if (error_) {
      error_ = error_type{};
    }
    return *this;
  }

  maybe& operator=(T value) {
    if (! valid_) {
      destroy();
      cr_moved_value(value);
    } else {
      assign_moved_value(value);
    }
    return *this;
  }

  maybe& operator=(error_type err) {
    if (valid_) {
      destroy();
      cr_error(std::move(err));
    } else {
      error_ = std::move(err);
    }
    return *this;
  }

  maybe& operator=(const maybe& other) {
    if (valid_ != other.valid_) {
      destroy();
      if (other.valid_)
        cr_value(other.value_);
      else
        cr_error(other.error_);
    } else if (valid_) {
      value_ = other.value_;
    } else {
      error_ = other.error_;
    }
    return *this;
  }

  maybe& operator=(maybe&& other) {
    if (valid_ != other.valid_) {
      destroy();
      if (other.valid_)
        cr_moved_value(other.value_);
      else
        cr_error(std::move(other.error_));
    } else if (valid_) {
      value_ = std::move(other.value_);
    } else {
      error_ = std::move(other.error_);
    }
    return *this;
  }

  /// Queries whether this instance holds a value.
  bool valid() const {
    return valid_;
  }

  /// Returns `valid()`.
  explicit operator bool() const {
    return valid();
  }

  /// Returns `! valid()`.
  bool operator!() const {
    return ! valid();
  }

  /// Returns the value.
  reference get() {
    CAF_ASSERT(valid());
    return value_;
  }

  /// Returns the value.
  const_reference get() const {
    CAF_ASSERT(valid());
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

  /// Returns whether this object holds a non-default `std::error_condition`.
  bool has_error() const {
    return ! valid() && static_cast<bool>(error());
  }

  /// Returns whether this objects holds neither a value nor an actual error.
  bool is_none() const {
    return ! valid() && static_cast<bool>(error()) == false;
  }

  /// Returns the error.
  const error_type& error() const {
    CAF_ASSERT(! valid());
    return error_;
  }

private:
  void destroy() {
    if (valid_)
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
    valid_ = true;
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
    valid_ = true;
    new (&value_) storage(std::forward<V>(x));
  }

  void cr_error(std::error_condition ec) {
    valid_ = false;
    new (&error_) error_type(std::move(ec));
  }

  bool valid_;
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

/// Returns `true` if `lhs` is valid and its value is equal to `rhs`.
template <class T, typename U>
bool operator==(const maybe<T>& lhs, const U& rhs) {
  return (lhs) ? *lhs == rhs : false;
}

/// Returns `true` if `rhs` is valid and its value is equal to `lhs`.
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

/// Returns `true` if `lhs` is invalid or its value is not equal to `rhs`.
/// @relates maybe
template <class T, typename U>
bool operator!=(const maybe<T>& lhs, const U& rhs) {
  return !(lhs == rhs);
}

/// Returns `true` if `rhs` is invalid or its value is not equal to `lhs`.
/// @relates maybe
template <class T, typename U>
bool operator!=(const T& lhs, const maybe<U>& rhs) {
  return !(lhs == rhs);
}

/// Returns `! val.valid() && val.error() == err`.
/// @relates maybe
template <class T>
bool operator==(const maybe<T>& val, const std::error_condition& err) {
  return ! val.valid() && val.error() == err;
}

/// Returns `! val.valid() && val.error() == err`.
/// @relates maybe
template <class T>
bool operator==(const std::error_condition& err, const maybe<T>& val) {
  return val == err;
}

/// Returns `val.valid() || val.error() != err`.
/// @relates maybe
template <class T>
bool operator!=(const maybe<T>& val, const std::error_condition& err) {
  return ! (val == err);
}

/// Returns `val.valid() || val.error() != err`.
/// @relates maybe
template <class T>
bool operator!=(const std::error_condition& err, const maybe<T>& val) {
  return ! (val == err);
}

/// Returns `val.is_none()`.
/// @relates maybe
template <class T>
bool operator==(const maybe<T>& val, const none_t&) {
  return val.is_none();
}

/// Returns `val.is_none()`.
/// @relates maybe
template <class T>
bool operator==(const none_t&, const maybe<T>& val) {
  return val.is_none();
}

/// Returns `! val.is_none()`.
/// @relates maybe
template <class T>
bool operator!=(const maybe<T>& val, const none_t&) {
  return ! val.is_none();
}

/// Returns `! val.is_none()`.
/// @relates maybe
template <class T>
bool operator!=(const none_t&, const maybe<T>& val) {
  return ! val.is_none();
}

} // namespace caf

#endif // CAF_MAYBE_HPP
