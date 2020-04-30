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
#include <memory>
#include <utility>

#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/error_code.hpp"
#include "caf/fwd.hpp"
#include "caf/is_error_code_enum.hpp"
#include "caf/message.hpp"
#include "caf/meta/load_callback.hpp"
#include "caf/meta/omittable_if_empty.hpp"
#include "caf/meta/save_callback.hpp"
#include "caf/meta/type_name.hpp"
#include "caf/none.hpp"
#include "caf/type_id.hpp"

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
/// The C++ standard library uses category singletons and virtual dispatching to
/// correlate error codes to descriptive strings. However, singletons are a poor
/// choice when it comes to serialization. CAF uses type IDs and meta objects
/// instead.
class CAF_CORE_EXPORT error : detail::comparable<error> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  error() noexcept = default;

  error(none_t) noexcept;

  error(error&&) noexcept = default;

  error& operator=(error&&) noexcept = default;

  error(const error&);

  error& operator=(const error&);

  template <class Enum, class = std::enable_if_t<is_error_code_enum_v<Enum>>>
  error(Enum code) : error(static_cast<uint8_t>(code), type_id_v<Enum>) {
    // nop
  }

  template <class Enum, class = std::enable_if_t<is_error_code_enum_v<Enum>>>
  error(Enum code, message context)
    : error(static_cast<uint8_t>(code), type_id_v<Enum>, std::move(context)) {
    // nop
  }

  template <class Enum>
  error(error_code<Enum> code) : error(to_integer(code), type_id_v<Enum>) {
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

  // -- properties -------------------------------------------------------------

  /// Returns the category-specific error code, whereas `0` means "no error".
  /// @pre `*this != none`
  uint8_t code() const noexcept {
    return data_->code;
  }

  /// Returns the ::type_id of the category for this error.
  /// @pre `*this != none`
  type_id_t category() const noexcept {
    return data_->category;
  }

  /// Returns context information to this error.
  /// @pre `*this != none`
  const message& context() const noexcept {
    return data_->context;
  }

  /// Returns `*this != none`.
  explicit operator bool() const noexcept {
    return data_ != nullptr;
  }

  /// Returns `*this == none`.
  bool operator!() const noexcept {
    return data_ == nullptr;
  }

  int compare(const error&) const noexcept;

  int compare(uint8_t code, type_id_t category) const noexcept;

  // -- static convenience functions -------------------------------------------

  /// @cond PRIVATE

  static error eval() {
    return error{};
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
          x.data_.reset();
          if constexpr (std::is_same<result_type, void>::value)
            return;
          else
            return result_type{};
        }
        if (!x.data_)
          x.data_.reset(new data);
        x.data_->code = code;
        return f(x.data_->category, x.data_->context);
      });
      return f(code, cb);
    }
  }

private:
  // -- constructors, destructors, and assignment operators --------------------

  error(uint8_t code, type_id_t category);

  error(uint8_t code, type_id_t category, message context);

  // -- nested classes ---------------------------------------------------------

  struct data {
    uint8_t code;
    type_id_t category;
    message context;
  };

  // -- member variables -------------------------------------------------------

  std::unique_ptr<data> data_;
};

/// @relates error
CAF_CORE_EXPORT std::string to_string(const error& x);

/// @relates error
template <class Enum>
std::enable_if_t<is_error_code_enum_v<Enum>, error> make_error(Enum code) {
  return error{code};
}

/// @relates error
template <class Enum, class T, class... Ts>
std::enable_if_t<is_error_code_enum_v<Enum>, error>
make_error(Enum code, T&& x, Ts&&... xs) {
  return error{code, make_message(std::forward<T>(x), std::forward<Ts>(xs)...)};
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
template <class Enum>
std::enable_if_t<is_error_code_enum_v<Enum>, bool>
operator==(const error& x, Enum y) {
  auto code = static_cast<uint8_t>(y);
  return code == 0 ? !x
                   : x && x.code() == code && x.category() == type_id_v<Enum>;
}

/// @relates error
template <class Enum>
std::enable_if_t<is_error_code_enum_v<Enum>, bool>
operator==(Enum x, const error& y) {
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
template <class Enum>
std::enable_if_t<is_error_code_enum_v<Enum>, bool>
operator!=(const error& x, Enum y) {
  return !(x == y);
}

/// @relates error
template <class Enum>
std::enable_if_t<is_error_code_enum_v<Enum>, bool>
operator!=(Enum x, const error& y) {
  return !(x == y);
}

} // namespace caf
