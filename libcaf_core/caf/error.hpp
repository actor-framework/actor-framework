/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#include <cstdint>

#include "caf/atom.hpp"
#include "caf/detail/comparable.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/fwd.hpp"
#include "caf/meta/load_callback.hpp"
#include "caf/meta/omittable_if_empty.hpp"
#include "caf/meta/type_name.hpp"
#include "caf/none.hpp"

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
using enable_if_has_make_error_t = typename std::enable_if<
  has_make_error<T>::value, U>::type;

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
  // -- constructors, destructors, and assignment operators --------------------

  error() noexcept;
  error(none_t) noexcept;

  error(error&&) noexcept;
  error& operator=(error&&) noexcept;

  error(const error&);
  error& operator=(const error&);

  error(uint8_t x, atom_value y);
  error(uint8_t x, atom_value y, message z);

  template <class E, class = enable_if_has_make_error_t<E>>
  error(E error_value) : error(make_error(error_value)) {
    // nop
  }

  template <class E, class = enable_if_has_make_error_t<E>>
  error& operator=(E error_value) {
    auto tmp = make_error(error_value);
    std::swap(data_, tmp.data_);
    return *this;
  }

  ~error();

  // -- observers --------------------------------------------------------------

  /// Returns the category-specific error code, whereas `0` means "no error".
  /// @pre `*this != none`
  uint8_t code() const noexcept;

  /// Returns the category of this error.
  /// @pre `*this != none`
  atom_value category() const noexcept;

  /// Returns context information to this error.
  /// @pre `*this != none`
  const message& context() const noexcept;

  /// Returns `*this != none`.
  explicit operator bool() const noexcept {
    return data_ != nullptr;
  }

  /// Returns `*this == none`.
  bool operator!() const noexcept {
    return data_ == nullptr;
  }

  int compare(const error&) const noexcept;

  int compare(uint8_t x, atom_value y) const noexcept;

  // -- modifiers --------------------------------------------------------------

  /// Returns the category-specific error code, whereas `0` means "no error".
  /// @pre `*this != none`
  uint8_t& code() noexcept;

  /// Returns the category of this error.
  /// @pre `*this != none`
  atom_value& category() noexcept;

  /// Returns context information to this error.
  /// @pre `*this != none`
  message& context() noexcept;

  /// Sets the error code to 0.
  void clear() noexcept;

  /// Assigns new code and category to this error.
  void assign(uint8_t code, atom_value category);

  /// Assigns new code, category and context to this error.
  void assign(uint8_t code, atom_value category, message context);

  // -- static convenience functions -------------------------------------------

  /// @cond PRIVATE

  static error eval() {
    return none;
  }

  template <class F, class... Fs>
  static error eval(F&& f, Fs&&... fs) {
    if (auto err = f())
      return err;
    return eval(std::forward<Fs>(fs)...);
  }

  /// @endcond

  // -- nested classes ---------------------------------------------------------

  struct data;

  struct inspect_helper {
    uint8_t code;
    error& err;
  };

private:
  // -- member variables -------------------------------------------------------

  data* data_;
};

// -- operators and free functions ---------------------------------------------

/// @relates error
template <class Inspector>
detail::enable_if_t<Inspector::reads_state, typename Inspector::result_type>
inspect(Inspector& f, const error& x) {
  if (x)
    return f(meta::type_name("error"), x.code(), x.category(),
             meta::omittable_if_empty(), x.context());
  return f(meta::type_name("error"), uint8_t{0});
}

template <class Inspector>
detail::enable_if_t<Inspector::writes_state, typename Inspector::result_type>
inspect(Inspector& f, error::inspect_helper& x) {
  if (x.code == 0) {
    x.err.clear();
    return f();
  }
  x.err.assign(x.code, atom(""));
  return f(x.err.category(), x.err.context());
}

/// @relates error
template <class Inspector>
detail::enable_if_t<Inspector::writes_state, typename Inspector::result_type>
inspect(Inspector& f, error& x) {
  error::inspect_helper helper{0, x};
  return f(helper.code, helper);
}

/// @relates error
std::string to_string(const error& x);

/// @relates error
inline bool operator==(const error& x, none_t) {
  return !x;
}

/// @relates error
inline bool operator==(none_t, const error& x) {
  return !x;
}

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
inline bool operator!=(const error& x, none_t) {
  return static_cast<bool>(x);
}

/// @relates error
inline bool operator!=(none_t, const error& x) {
  return static_cast<bool>(x);
}

/// @relates error
template <class E, class = enable_if_has_make_error_t<E>>
bool operator!=(const error& x, E y) {
  return !(x == y);
}

/// @relates error
template <class E, class = enable_if_has_make_error_t<E>>
bool operator!=(E x, const error& y) {
  return !(x == y);
}

} // namespace caf
