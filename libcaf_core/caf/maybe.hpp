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

#include "caf/none.hpp"
#include "caf/unit.hpp"
#include "caf/error.hpp"
#include "caf/config.hpp"
#include "caf/deep_to_string.hpp"

#include "caf/detail/safe_equal.hpp"

namespace caf {

/// Represents a computation returning either `T` or `std::error_condition`.
/// Note that the error condition might be default-constructed. In this case,
/// a `maybe` represents simply `none`. Hence, this type has three possible
/// states:
/// - A value of `T` is available
///   + `valid()` returns `true`
///   + `empty()` returns `false`
///   + `invalid()` returns `false`
/// -  No value is available but no error occurred
///   + `valid()` returns `false`
///   + `empty()` returns `true`
///   + `invalid()` returns `false`
/// - An error occurred (no value available)
///   + `valid()` returns `false`
///   + `empty()` returns `false`
///   + `invalid()` returns `true`
template <class T>
class maybe {
public:
  using type = typename std::remove_reference<T>::type;
  using reference = type&;
  using const_reference = const type&;
  using pointer = type*;
  using const_pointer = const type*;
  using error_type = caf::error;

  /// Type for storing values.
  using storage =
    typename std::conditional<
      std::is_reference<T>::value,
      std::reference_wrapper<type>,
      T
    >::type;

  /// Creates an instance representing an error.
  maybe(const error_type& x) {
    cr_error(x);
  }

  /// Creates an instance representing an error.
  maybe(error_type&& x) {
    cr_error(std::move(x));
  }

  /// Creates an instance representing a value from `x`.
  template <class U,
            class = typename std::enable_if<
                      std::is_convertible<U, T>::value
                    >::type>
  maybe(U&& x) {
    cr_value(std::forward<U>(x));
  }

  template <class U0, class U1, class... Us>
  maybe(U0&& x0, U1&& x1, Us&&... xs) {
    flag_ = available_flag;
    new (&value_) storage(std::forward<U0>(x0), std::forward<U1>(x1),
                          std::forward<Us>(xs)...);
  }

  /// Creates an instance representing an error
  /// from a type offering the free function `make_error`.
  template <class E,
            class = typename std::enable_if<
                      std::is_same<
                        decltype(make_error(std::declval<const E&>())),
                        error
                      >::value
                    >::type>
  maybe(E error_enum) : maybe(make_error(error_enum)) {
    // nop
  }

  /// Creates an empty instance.
  maybe() : flag_(empty_flag) {
    // nop
  }

  /// Creates an empty instance.
  maybe(const none_t&) : flag_(empty_flag) {
    // nop
  }

  maybe(const maybe& other) {
    if (other.valid())
      cr_value(other.value_);
    else
      cr_error(other);
  }

  maybe(maybe&& other) {
    if (other.valid())
      cr_value(std::move(other.value_));
    else
      cr_error(std::move(other));
  }

  template <class U>
  maybe(maybe<U>&& other) {
    static_assert(std::is_convertible<U, T>::value, "U not convertible to T");
    if (other)
      cr_value(std::move(*other));
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
    if (! empty()) {
      destroy();
      flag_ = empty_flag;
    }
    return *this;
  }

  template <class U,
            class = typename std::enable_if<
                      std::is_convertible<U, T>::value
                    >::type>
  maybe& operator=(U&& x) {
    if (! valid()) {
      destroy();
      cr_value(std::forward<U>(x));
    } else {
      assign_value(std::forward<U>(x));
    }
    return *this;
  }

  maybe& operator=(const error_type& err) {
    destroy();
    cr_error(err);
    return *this;
  }

  maybe& operator=(error_type&& err) {
    destroy();
    cr_error(std::move(err));
    return *this;
  }

  template <class E,
            class = typename std::enable_if<
                      std::is_same<
                        decltype(make_error(std::declval<const E&>())),
                        error_type
                      >::value
                    >::type>
  maybe& operator=(E error_enum) {
    return *this = make_error(error_enum);
  }

  maybe& operator=(maybe&& other) {
    if (other.valid())
      *this = std::move(*other);
    else
      cr_error(std::move(other));
    return *this;
  }

  maybe& operator=(const maybe& other) {
    if (other.valid())
      *this = *other;
    else
      cr_error(other);
  }

  template <class U>
  maybe& operator=(maybe<U>&& other) {
    static_assert(std::is_convertible<U, T>::value, "U not convertible to T");
    if (other.valid())
      *this = std::move(*other);
    else
      cr_error(std::move(other));
    return *this;
  }

  template <class U>
  maybe& operator=(const maybe<U>& other) {
    static_assert(std::is_convertible<U, T>::value, "U not convertible to T");
    if (other.valid())
      *this = *other;
    else
      cr_error(other);
  }

  /// Queries whether this instance holds a value.
  bool valid() const {
    return flag_ == available_flag;
  }

  /// Returns `available()`.
  explicit operator bool() const {
    return valid();
  }

  /// Returns `! available()`.
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

  /// Returns whether this objects holds neither a value nor an actual error.
  bool empty() const {
    return flag_ == empty_flag;
  }

  bool invalid() const {
    return (flag_ & error_code_mask) != 0;
  }

  /// Creates an error object.
  error_type error() const {
    if (valid())
      return {};
    if (has_error_context())
      return {error_code(), extended_error_->first, extended_error_->second};
    return {error_code(), error_category_};
  }

  uint8_t error_code() const {
    return static_cast<uint8_t>(flag_ & error_code_mask);
  }

  atom_value error_category() const {
    if (valid())
      return atom("");
    if (has_error_context())
      return extended_error_->first;
    return error_category_;
  }

private:
  bool has_error_context() const {
    return (flag_ & error_context_mask) != 0;
  }

  void destroy() {
    if (valid())
      value_.~storage();
    else if (has_error_context())
      delete extended_error_;
  }

  template <class V>
  void assign_value(V&& x) {
    using x_type = typename std::remove_reference<V>::type;
    using fwd_type =
      typename std::conditional<
        std::is_rvalue_reference<decltype(x)>::value
        && ! std::is_reference<T>::value,
        x_type&&,
        x_type&
      >::type;
    value_ = static_cast<fwd_type>(x);
  }

  template <class V>
  void cr_value(V&& x) {
    using x_type = typename std::remove_reference<V>::type;
    using fwd_type =
      typename std::conditional<
        std::is_rvalue_reference<decltype(x)>::value
        && ! std::is_reference<T>::value,
        x_type&&,
        x_type&
      >::type;
    flag_ = available_flag;
    new (&value_) storage(static_cast<fwd_type>(x));
  }

  template <class U>
  void cr_error(const maybe<U>& other) {
    flag_ = other.flag_;
    if (has_error_context())
      extended_error_ = new extended_error(*other.extended_error_);
    else
      error_category_ = other.error_category_;
  }

  template <class U>
  void cr_error(maybe<U>&& other) {
    flag_ = other.flag_;
    if (has_error_context())
      extended_error_ = other.extended_error_;
    else
      error_category_ = other.error_category_;
    other.flag_ = empty_flag; // take ownership of extended_error_
  }

  void cr_error(const error_type& x) {
    flag_ = x.compress_code_and_size();
    if (has_error_context())
      extended_error_ = new extended_error(x.category(), x.context());
    else
      error_category_ = x.category();
  }

  void cr_error(error_type&& x) {
    flag_ = x.compress_code_and_size();
    if (has_error_context())
      extended_error_ = new extended_error(x.category(),
                                           std::move(x.context()));
    else
      error_category_ = x.category();
  }

  static constexpr uint32_t available_flag = 0x80000000;
  static constexpr uint32_t empty_flag = 0x00000000;
  static constexpr uint32_t error_code_mask = 0x000000FF;
  static constexpr uint32_t error_context_mask = 0x7FFFFF00;

  using extended_error = std::pair<atom_value, std::string>;

  // stores the availability flag (1bit), context string size (23 bit),
  // and error code (8 bit).
  uint32_t flag_;
  union {
    // if flag == available_flag
    storage value_;
    // if (flag & error_context_mask) == 0
    atom_value error_category_;
    // if (flag & error_context_mask) != 0
    extended_error* extended_error_;
  };
};


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
  using error_type = caf::error;

  maybe() = default;

  maybe(const unit_t&) {
    // nop
  }

  maybe(const none_t&) {
    // nop
  }

  maybe(error_type err) : error_(std::move(err)) {
    // nop
  }

  template <class E,
            class = typename std::enable_if<
                      std::is_same<
                        decltype(make_error(std::declval<const E&>())),
                        error_type
                      >::value
                    >::type>
  maybe(E error_code) : error_(make_error(error_code)) {
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
                      std::is_same<
                        decltype(make_error(std::declval<const E&>())),
                        error_type
                      >::value
                    >::type>
  maybe& operator=(E error_code) {
    return *this = make_error(error_code);
  }

  bool valid() const {
    return false;
  }

  explicit operator bool() const {
    return valid();
  }

  bool operator!() const {
    return ! valid();
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

/// Allows element-wise access of STL-compatible tuples.
/// @relates maybe
template <size_t X, class T>
maybe<typename std::tuple_element<X, T>::type&> get(maybe<T>& xs) {
  if (xs)
    return std::get<X>(*xs);
  return xs.error();
}

/// Allows element-wise access of STL-compatible tuples.
/// @relates maybe
template <size_t X, class T>
maybe<const typename std::tuple_element<X, T>::type&> get(const maybe<T>& xs) {
  if (xs)
    return std::get<X>(*xs);
  return xs.error();
}

/// Returns `true` if both objects represent either the same
/// value or the same error, `false` otherwise.
/// @relates maybe
template <class T, typename U>
bool operator==(const maybe<T>& x, const maybe<U>& y) {
  if (x)
    return (y) ? detail::safe_equal(*x, *y) : false;
  if (x.empty() && y.empty())
    return true;
  if (! y)
    return x.error_code() == y.error_code()
           && x.error_category() == y.error_category();
  return false;
}

/// Returns `true` if `lhs` is available and its value is equal to `rhs`.
template <class T, typename U>
bool operator==(const maybe<T>& x, const U& y) {
  return (x) ? *x == y : false;
}

/// Returns `true` if `rhs` is available and its value is equal to `lhs`.
/// @relates maybe
template <class T, typename U>
bool operator==(const T& x, const maybe<U>& y) {
  return y == x;
}

/// Returns `true` if the objects represent different
/// values or errors, `false` otherwise.
/// @relates maybe
template <class T, typename U>
bool operator!=(const maybe<T>& x, const maybe<U>& y) {
  return !(x == y);
}

/// Returns `true` if `lhs` is not available or its value is not equal to `rhs`.
/// @relates maybe
template <class T, typename U>
bool operator!=(const maybe<T>& x, const U& y) {
  return !(x == y);
}

/// Returns `true` if `rhs` is not available or its value is not equal to `lhs`.
/// @relates maybe
template <class T, typename U>
bool operator!=(const T& x, const maybe<U>& y) {
  return !(x == y);
}

/// Returns `! val.available() && val.error() == err`.
/// @relates maybe
template <class T>
bool operator==(const maybe<T>& x, const error& y) {
  return x.invalid() && y.compare(x.error_code(), x.error_category()) == 0;
}

/// Returns `! val.available() && val.error() == err`.
/// @relates maybe
template <class T>
bool operator==(const error& x, const maybe<T>& y) {
  return y == x;
}

/// Returns `val.available() || val.error() != err`.
/// @relates maybe
template <class T>
bool operator!=(const maybe<T>& x, const error& y) {
  return ! (x == y);
}

/// Returns `val.available() || val.error() != err`.
/// @relates maybe
template <class T>
bool operator!=(const error& x, const maybe<T>& y) {
  return ! (y == x);
}

/// Returns `val.empty()`.
/// @relates maybe
template <class T>
bool operator==(const maybe<T>& x, const none_t&) {
  return x.empty();
}

/// Returns `val.empty()`.
/// @relates maybe
template <class T>
bool operator==(const none_t&, const maybe<T>& x) {
  return x.empty();
}

/// Returns `! val.empty()`.
/// @relates maybe
template <class T>
bool operator!=(const maybe<T>& x, const none_t&) {
  return ! x.empty();
}

/// Returns `! val.empty()`.
/// @relates maybe
template <class T>
bool operator!=(const none_t&, const maybe<T>& x) {
  return ! x.empty();
}

/// @relates maybe
template <class Processor, class T>
typename std::enable_if<Processor::is_saving::value>::type
serialize(Processor& sink, maybe<T>& x, const unsigned int) {
  uint8_t flag = x.empty() ? 0 : (x.valid() ? 1 : 2);
  sink & flag;
  if (x.valid())
    sink & *x;
  if (x.invalid()) {
    auto err = x.error();
    sink & err;
  }
}

/// @relates maybe
template <class Processor, class T>
typename std::enable_if<Processor::is_loading::value>::type
serialize(Processor& source, maybe<T>& x, const unsigned int) {
  uint8_t flag;
  source & flag;
  switch (flag) {
    case 1: {
      T value;
      source & value;
      x = std::move(value);
      break;
    }
    case 2: {
      error err;
      source & err;
      x = std::move(err);
      break;
    }
    default:
      x = none;
  }
}

/// @relates maybe
template <class T>
std::string to_string(const maybe<T>& x) {
  if (x)
    return deep_to_string(*x);
  if (x.empty())
    return "<none>";
  return to_string(x.error());
}

} // namespace caf

#endif // CAF_MAYBE_HPP
