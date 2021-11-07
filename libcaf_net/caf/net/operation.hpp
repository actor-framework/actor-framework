// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>
#include <string>
#include <type_traits>

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/is_error_code_enum.hpp"

namespace caf::net {

/// Values for representing bitmask of I/O operations.
enum class operation {
  none = 0b0000,
  read = 0b0001,
  write = 0b0010,
  block_read = 0b0100,
  block_write = 0b1000,
  read_write = 0b0011,
  read_only = 0b1001,
  write_only = 0b0110,
  shutdown = 0b1100,
};

/// @relates operation
[[nodiscard]] constexpr int to_integer(operation x) noexcept {
  return static_cast<int>(x);
}

/// Adds the `read` flag to `x` unless the `block_read` bit is set.
/// @relates operation
[[nodiscard]] constexpr operation add_read_flag(operation x) noexcept {
  if (auto bits = to_integer(x); !(bits & 0b0100))
    return static_cast<operation>(bits | 0b0001);
  else
    return x;
}

/// Adds the `write` flag to `x` unless the `block_write` bit is set.
/// @relates operation
[[nodiscard]] constexpr operation add_write_flag(operation x) noexcept {
  if (auto bits = to_integer(x); !(bits & 0b1000))
    return static_cast<operation>(bits | 0b0010);
  else
    return x;
}

/// Removes the `read` flag from `x`.
/// @relates operation
[[nodiscard]] constexpr operation remove_read_flag(operation x) noexcept {
  return static_cast<operation>(to_integer(x) & 0b1110);
}

/// Removes the `write` flag from `x`.
/// @relates operation
[[nodiscard]] constexpr operation remove_write_flag(operation x) noexcept {
  return static_cast<operation>(to_integer(x) & 0b1101);
}

/// Adds the `block_read` flag to `x` and removes the `read` flag if present.
/// @relates operation
[[nodiscard]] constexpr operation block_reads(operation x) noexcept {
  auto bits = to_integer(x) | 0b0100;;
  return static_cast<operation>(bits & 0b1110);
}

/// Adds the `block_write` flag to `x` and removes the `write` flag if present.
/// @relates operation
[[nodiscard]] constexpr operation block_writes(operation x) noexcept {
  auto bits = to_integer(x) | 0b1000;;
  return static_cast<operation>(bits & 0b1101);
}

/// Returns whether the read flag is present in `x`.
/// @relates operation
[[nodiscard]] constexpr bool is_reading(operation x) noexcept {
  return (to_integer(x) & 0b0001) == 0b0001;
}

/// Returns whether the write flag is present in `x`.
/// @relates operation
[[nodiscard]] constexpr bool is_writing(operation x) noexcept {
  return (to_integer(x) & 0b0010) == 0b0010;
}

/// Returns `!is_reading(x) && !is_writing(x)`.
/// @relates operation
[[nodiscard]] constexpr bool is_idle(operation x) noexcept {
  return (to_integer(x) & 0b0011) == 0b0000;
}

/// Returns whether the block read flag is present in `x`.
/// @relates operation
[[nodiscard]] constexpr bool is_read_blocked(operation x) noexcept {
  return (to_integer(x) & 0b0100) == 0b0100;
}

/// Returns whether the block write flag is present in `x`.
/// @relates operation
[[nodiscard]] constexpr bool is_write_blocked(operation x) noexcept {
  return (to_integer(x) & 0b1000) == 0b1000;
}

/// @relates operation
CAF_NET_EXPORT std::string to_string(operation x);

/// @relates operation
CAF_NET_EXPORT bool from_string(string_view, operation&);

/// @relates operation
CAF_NET_EXPORT bool from_integer(std::underlying_type_t<operation>, operation&);

/// @relates operation
template <class Inspector>
bool inspect(Inspector& f, operation& x) {
  return default_enum_inspect(f, x);
}

} // namespace caf::net
