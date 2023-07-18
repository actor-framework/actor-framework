// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/fwd.hpp"
#include "caf/is_error_code_enum.hpp"
#include "caf/none.hpp"

#include <string>
#include <type_traits>

namespace caf {

/// A lightweight wrapper around an error code enum.
template <class Enum>
class error_code {
public:
  using enum_type = Enum;

  using underlying_type = std::underlying_type_t<Enum>;

  static_assert(is_error_code_enum_v<Enum>);

  static_assert(std::is_same_v<underlying_type, uint8_t>);

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

  friend constexpr underlying_type to_integer(error_code x) noexcept {
    return static_cast<std::underlying_type_t<Enum>>(x.value());
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
