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

#ifndef CAF_ERROR_HPP
#define CAF_ERROR_HPP

#include <cstdint>

#include "caf/fwd.hpp"
#include "caf/atom.hpp"
#include "caf/message.hpp"

#include "caf/detail/comparable.hpp"

namespace caf {

class error;

/// Evaluates to true if `T` is an enum with a free function
/// `make_error` for converting it to an `error`.
template <class T>
struct has_make_error {
private:
  template <class U>
  static auto test_make_error(U* x) -> decltype(make_error(*x));

  template <class U>
  static auto test_make_error(...) -> void;

  using type = decltype(test_make_error<T>(nullptr));

public:
  static constexpr bool value = std::is_enum<T>::value
                                && std::is_same<error, type>::value;
};

/// Convenience alias for `std::enable_if<has_make_error<T>::value, U>::type`.
template <class T, class U = void>
using enable_if_has_make_error_t
  = typename std::enable_if<has_make_error<T>::value, U>::type;

/// A serializable type for storing error codes with category and optional,
/// human-readable context information. Unlike error handling classes from
/// the C++ standard library, this type is serializable. It consists of an
/// 8-bit code, a 64-bit atom constant, plus optionally a ::message to store
/// additional information.
///
/// # Why not `std::error_code` or `std::error_condition`?
///
/// First, the standard does *not* define the values for `std::errc`.
/// This means serializing error conditions (which are meant to be portable)
/// is not safe in a distributed setting unless all machines are running the
/// same operating system and version of the C++ standard library.
///
/// Second, the standard library primitives, unlike exceptions, do not offer
/// an API for attaching additional context to an error. The error handling API
/// offered by the standard is meant to wrap C system calls in a (source code)
/// portable way. In a distributed setting, an error may not occur locally.
/// In this case, an error code and category alone is often not satisfactory
/// information when signalling errors back to end users. The additional
/// context also enables *composition* of errors by modifying the message
/// details as needed.
///
/// # Why is there no `string()` member function?
///
/// The C++ standard library uses category singletons and virtual dispatching
/// to correlate error codes to descriptive strings. However, singletons are
/// a poor choice when it comes to serialization. CAF uses atoms for
/// categories instead and requires users to register custom error categories
/// to the actor system. This makes the actor system the natural instance for
/// rendering error messages via `actor_system::render(const error&)`.
class error : detail::comparable<error> {
public:
  error() noexcept;
  error(error&&) noexcept = default;
  error(const error&) noexcept = default;
  error& operator=(error&&) noexcept = default;
  error& operator=(const error&) noexcept = default;

  error(uint8_t code, atom_value category) noexcept;
  error(uint8_t code, atom_value category, message msg) noexcept;

  template <class E, class = enable_if_has_make_error_t<E>>
  error(E error_value) : error(make_error(error_value)) {
    // nop
  }

  /// Returns the category-specific error code, whereas `0` means "no error".
  inline uint8_t code() const noexcept {
    return code_;
  }

  /// Returns the category of this error.
  inline atom_value category() const noexcept {
    return category_;
  }

  /// Returns optional context information to this error.
  inline message& context() noexcept {
    return context_;
  }

  /// Returns optional context information to this error.
  inline const message& context() const noexcept {
    return context_;
  }

  /// Returns `code() != 0`.
  inline explicit operator bool() const noexcept {
    return code_ != 0;
  }

  /// Returns `code() == 0`.
  inline bool operator!() const noexcept {
    return code_ == 0;
  }

  /// Sets the error code to 0.
  inline void clear() noexcept {
    code_ = 0;
    context_.reset();
  }

  friend void serialize(serializer& sink, error& x, const unsigned int);

  friend void serialize(deserializer& source, error& x, const unsigned int);

  int compare(const error&) const noexcept;

  int compare(uint8_t code, atom_value category) const noexcept;

private:
  uint8_t code_;
  atom_value category_;
  message context_;
};

/// @relates error
template <class E, class = enable_if_has_make_error_t<E>>
bool operator==(const error& x, E y) {
  return x == make_error(y);
}

/// @relates error
template <class E, class = enable_if_has_make_error_t<E>>
bool operator==(E x, const error& y) {
  return make_error(x) == y;
}

/// @relates error
template <class E, class = enable_if_has_make_error_t<E>>
bool operator!=(const error& x, E y) {
  return ! (x == y);
}

/// @relates error
template <class E, class = enable_if_has_make_error_t<E>>
bool operator!=(E x, const error& y) {
  return ! (x == y);
}

/// @relates error
std::string to_string(const error& x);

} // namespace caf

#endif // CAF_ERROR_HPP
