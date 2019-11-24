/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
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

#include <string>
#include <type_traits>

#include "caf/fwd.hpp"
#include "caf/none.hpp"

namespace caf {

/// A lightweight wrapper around an error code enum.
template <class Enum>
class error_code {
public:
  using enum_type = Enum;

  using underlying_type = std::underlying_type_t<enum_type>;

  constexpr error_code() noexcept : value_(static_cast<Enum>(0)) {
    // nop
  }

  constexpr error_code(none_t) noexcept : value_(static_cast<Enum>(0)) {
    // nop
  }

  constexpr error_code(enum_type value) noexcept : value_(value) {
    // nop
  }

  constexpr error_code(const error_code&) noexcept = default;

  constexpr error_code& operator=(const error_code&) noexcept = default;

  error_code& operator=(Enum value) noexcept {
    value_ = value;
    return *this;
  }

  constexpr explicit operator bool() const noexcept {
    return static_cast<underlying_type>(value_) != 0;
  }

  constexpr enum_type value() const noexcept {
    return value_;
  }

private:
  enum_type value_;
};

/// Converts `x` to a string if `Enum` provides a `to_string` function.
template <class Enum>
auto to_string(error_code<Enum> x) -> decltype(to_string(x.value())) {
  return to_string(x.value());
}

} // namespace caf
