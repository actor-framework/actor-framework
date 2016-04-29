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

/// A serializable type for storing error codes with category and optional,
/// human-readable context information. Unlike error handling classes from
/// the C++ standard library, this type is serializable and
/// optimized for (on-the-wire) size. Note, error codes are limited to 255
/// and the string for storing context information is limited to
/// 8,388,608 characters.
///
/// # Design goals
///
/// The error type in CAF is meant to allow efficient packing of errors
/// in either `maybe` or on the wire. For this purpose, CAF limits the
/// error code to 255 to allow storing the context size along the code
/// in a 32-bit integer (1 bit invaldity flag, 23 bit context size, 8 bit code;
/// if the validity flag is 1, then the error is invalid and has no category).
///
/// # Why not `std::error_code` or `std::error_condition`?
///
/// First, the standard does *not* define the values for `std::errc`.
/// This means serializing error conditions (which are meant to be portable)
/// is not safe in a distributed setting unless all machines are running the
/// same operating system and version of the C++ standard library.
///
/// Second, the standard library primitives, unlike exceptions, do not offer
/// an API for storing additional context to an error. The error handling API
/// offered by the standard is meant to wrap C system calls and defines
/// in a (source code) portable way. In a distributed setting, an error does
/// not occur locally. Error code and category often are simply not
/// satisfactory information when signalling errors back to end users.
///
/// # Why is there no `message()` member function?
///
/// The C++ standard library uses category singletons and virtual dispatching
/// to correlate error codes to descriptive strings. However, singletons are
/// a poor choice when it comes to serialization. CAF uses atoms for
/// categories instead and requires users to register custom error categories
/// to the actor system. This makes the actor system the natural instance for
/// rendering error messages via `actor_system::render(const error&)`.
class error : detail::comparable<error> {
public:
  error();
  error(error&&) = default;
  error(const error&) = default;
  error& operator=(error&&) = default;
  error& operator=(const error&) = default;
  error(uint8_t code, atom_value category, message context = message{});

  template <class E,
            class = typename std::enable_if<
                      std::is_same<
                        decltype(make_error(std::declval<const E&>())),
                        error
                      >::value
                    >::type>
  error(E error_value) : error(make_error(error_value)) {
    // nop
  }

  /// Returns the category-specific error code, whereas `0` means "no error".
  uint8_t code() const;

  /// Returns the category of this error.
  atom_value category() const;

  /// Returns optional context information to this error.
  message& context();

  /// Returns optional context information to this error.
  const message& context() const;

  /// Returns `code() != 0`.
  inline explicit operator bool() const noexcept {
    return code_ != 0;
  }

  /// Returns `code() == 0`.
  inline bool operator!() const noexcept {
    return code_ == 0;
  }

  /// Sets the error code to 0.
  void clear();

  /// Creates a flag storing the context size and the error code.
  uint32_t compress_code_and_size() const;

  friend void serialize(serializer& sink, error& x, const unsigned int);

  friend void serialize(deserializer& source, error& x, const unsigned int);

  int compare(uint8_t code, atom_value category) const;

  int compare(const error&) const;

private:
  uint8_t code_;
  atom_value category_;
  message context_;
};

/// @relates error
template <class E,
          class = typename std::enable_if<
                    std::is_same<
                      decltype(make_error(std::declval<const E&>())),
                      error
                    >::value
                  >::type>
bool operator==(const error& x, E y) {
  return x == make_error(y);
}

/// @relates error
template <class E,
          class = typename std::enable_if<
                    std::is_same<
                      decltype(make_error(std::declval<const E&>())),
                      error
                    >::value
                  >::type>
bool operator==(E x, const error& y) {
  return make_error(x) == y;
}

/// @relates error
template <class E,
          class = typename std::enable_if<
                    std::is_same<
                      decltype(make_error(std::declval<const E&>())),
                      error
                    >::value
                  >::type>
bool operator!=(const error& x, E y) {
  return ! (x == y);
}

/// @relates error
template <class E,
          class = typename std::enable_if<
                    std::is_same<
                      decltype(make_error(std::declval<const E&>())),
                      error
                    >::value
                  >::type>
bool operator!=(E x, const error& y) {
  return ! (x == y);
}

/// @relates error
std::string to_string(const error& x);

} // namespace caf

#endif // CAF_ERROR_HPP
