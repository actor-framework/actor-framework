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
#include <functional>
#include <utility>

#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/error_category.hpp"
#include "caf/error_code.hpp"
#include "caf/fwd.hpp"
#include "caf/meta/load_callback.hpp"
#include "caf/meta/omittable_if_empty.hpp"
#include "caf/meta/save_callback.hpp"
#include "caf/meta/type_name.hpp"
#include "caf/none.hpp"

namespace caf {

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
class CAF_CORE_EXPORT error : detail::comparable<error> {
public:
  // -- member types -----------------------------------------------------------

  using inspect_fun
    = std::function<error(meta::type_name_t, uint8_t&, uint8_t&,
                          meta::omittable_if_empty_t, message&)>;

  // -- constructors, destructors, and assignment operators --------------------

  error() noexcept;

  error(none_t) noexcept;

  error(error&&) noexcept;

  error& operator=(error&&) noexcept;

  error(const error&);

  error& operator=(const error&);

  error(uint8_t x, uint8_t y);

  error(uint8_t x, uint8_t y, message z);

  template <class Enum, uint8_t Category = error_category<Enum>::value>
  error(Enum error_value) : error(static_cast<uint8_t>(error_value), Category) {
    // nop
  }

  template <class Enum>
  error(error_code<Enum> code) : error(code.value()) {
    // nop
  }

  template <class E>
  error& operator=(E error_value) {
    error tmp{error_value};
    std::swap(data_, tmp.data_);
    return *this;
  }

  template <class E>
  error& operator=(error_code<E> code) {
    return *this = code.value();
  }

  ~error();

  // -- observers --------------------------------------------------------------

  /// Returns the category-specific error code, whereas `0` means "no error".
  /// @pre `*this != none`
  uint8_t code() const noexcept;

  /// Returns the category of this error.
  /// @pre `*this != none`
  uint8_t category() const noexcept;

  /// Returns context information to this error.
  /// @pre `*this != none`
  const message& context() const noexcept;

  /// Returns `*this != none`.
  inline explicit operator bool() const noexcept {
    return data_ != nullptr;
  }

  /// Returns `*this == none`.
  inline bool operator!() const noexcept {
    return data_ == nullptr;
  }

  int compare(const error&) const noexcept;

  int compare(uint8_t x, uint8_t y) const noexcept;

  // -- modifiers --------------------------------------------------------------

  /// Returns context information to this error.
  /// @pre `*this != none`
  message& context() noexcept;

  /// Sets the error code to 0.
  void clear() noexcept;

  // -- static convenience functions -------------------------------------------

  /// @cond PRIVATE

  static inline error eval() {
    return none;
  }

  template <class F, class... Fs>
  static error eval(F&& f, Fs&&... fs) {
    auto x = f();
    return x ? x : eval(std::forward<Fs>(fs)...);
  }

  /// @endcond

  // -- friend functions -------------------------------------------------------

  template <class Inspector>
  friend auto inspect(Inspector& f, error& x) {
    using result_type = typename Inspector::result_type;
    if constexpr (Inspector::reads_state) {
      if (!x) {
        uint8_t code = 0;
        return f(code);
      }
      return f(x.code(), x.category(), x.context());
    } else {
      uint8_t code = 0;
      auto cb = meta::load_callback([&] {
        if (code == 0) {
          x.clear();
          if constexpr (std::is_same<result_type, void>::value)
            return;
          else
            return result_type{};
        }
        x.init();
        x.code_ref() = code;
        return f(x.category_ref(), x.context());
      });
      return f(code, cb);
    }
  }

private:
  // -- inspection support -----------------------------------------------------

  uint8_t& code_ref() noexcept;

  uint8_t& category_ref() noexcept;

  void init();

  // -- nested classes ---------------------------------------------------------

  struct data;

  // -- member variables -------------------------------------------------------

  data* data_;
};

/// @relates error
CAF_CORE_EXPORT std::string to_string(const error& x);

/// @relates error
template <class Enum>
error make_error(Enum code) {
  return error{static_cast<uint8_t>(code), error_category<Enum>::value};
}

/// @relates error
template <class Enum, class T, class... Ts>
error make_error(Enum code, T&& x, Ts&&... xs) {
  return error{static_cast<uint8_t>(code), error_category<Enum>::value,
               make_message(std::forward<T>(x), std::forward<Ts>(xs)...)};
}

/// @relates error
inline bool operator==(const error& x, none_t) {
  return !x;
}

/// @relates error
inline bool operator==(none_t, const error& x) {
  return !x;
}

/// @relates error
template <class Enum, uint8_t Category = error_category<Enum>::value>
bool operator==(const error& x, Enum y) {
  auto code = static_cast<uint8_t>(y);
  return code == 0 ? !x : x && x.category() == Category && x.code() == code;
}

/// @relates error
template <class Enum, uint8_t Category = error_category<Enum>::value>
bool operator==(Enum x, const error& y) {
  return y == x;
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
template <class Enum, uint8_t Category = error_category<Enum>::value>
bool operator!=(const error& x, Enum y) {
  return !(x == y);
}

/// @relates error
template <class Enum, uint8_t Category = error_category<Enum>::value>
bool operator!=(Enum x, const error& y) {
  return !(x == y);
}

} // namespace caf
