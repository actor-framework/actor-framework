// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/error.hpp"
#include "caf/error_code_enum.hpp"
#include "caf/expected.hpp"

#include <type_traits>

namespace caf {

/// A lightweight wrapper around an error code enum to make it comparable with
/// `error`, `unexpected`, and `expected`.
template <class Enum>
class error_code {
public:
  using enum_type = Enum;

  using underlying_type = std::underlying_type_t<Enum>;

  static_assert(error_code_enum<Enum>);

  static_assert(std::is_same_v<underlying_type, uint8_t>);

  constexpr error_code() noexcept : value_(static_cast<Enum>(0)) {
    // nop
  }

  explicit constexpr error_code(enum_type value) noexcept : value_(value) {
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

/// @relates error_code
template <class Enum>
error_code(Enum) -> error_code<Enum>;

/// Converts `x` to a string if `Enum` provides a `to_string` function.
template <class Enum>
auto to_string(error_code<Enum> x) -> decltype(to_string(x.value())) {
  return to_string(x.value());
}

template <class Enum>
constexpr bool operator==(error_code<Enum> lhs, error_code<Enum> rhs) noexcept {
  return lhs.value() == rhs.value();
}

template <class Enum>
constexpr bool operator!=(error_code<Enum> lhs, error_code<Enum> rhs) noexcept {
  return lhs.value() != rhs.value();
}

template <class T, class Enum>
bool operator==(const expected<T>& lhs, error_code<Enum> rhs) {
  if (lhs) {
    return false;
  }
  return lhs.error() == rhs.value();
}

template <class Enum, class T>
bool operator==(error_code<Enum> lhs, const expected<T>& rhs) {
  return rhs == lhs;
}

template <class T, class Enum>
bool operator!=(const expected<T>& lhs, error_code<Enum> rhs) {
  return !(lhs == rhs);
}

template <class Enum, class T>
bool operator!=(error_code<Enum> lhs, const expected<T>& rhs) {
  return !(lhs == rhs);
}

template <class Error, std::equality_comparable_with<Error> Enum>
bool operator==(const unexpected<Error>& lhs, error_code<Enum> rhs) {
  return lhs.error() == rhs.value();
}

template <class Error, std::equality_comparable_with<Error> Enum>
bool operator==(error_code<Enum> lhs, const unexpected<Error>& rhs) {
  return rhs == lhs;
}

template <class Error, std::equality_comparable_with<Error> Enum>
bool operator!=(const unexpected<Error>& lhs, error_code<Enum> rhs) {
  return !(lhs == rhs);
}

template <class Error, std::equality_comparable_with<Error> Enum>
bool operator!=(error_code<Enum> lhs, const unexpected<Error>& rhs) {
  return !(lhs == rhs);
}

} // namespace caf
