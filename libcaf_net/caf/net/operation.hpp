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
  none = 0x00,
  read = 0x01,
  write = 0x02,
  read_write = 0x03,
  shutdown = 0x04,
};

/// @relates operation
constexpr operation operator|(operation x, operation y) {
  return static_cast<operation>(static_cast<int>(x) | static_cast<int>(y));
}

/// @relates operation
constexpr operation operator&(operation x, operation y) {
  return static_cast<operation>(static_cast<int>(x) & static_cast<int>(y));
}

/// @relates operation
constexpr operation operator^(operation x, operation y) {
  return static_cast<operation>(static_cast<int>(x) ^ static_cast<int>(y));
}

/// @relates operation
constexpr operation operator~(operation x) {
  return static_cast<operation>(~static_cast<int>(x));
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
