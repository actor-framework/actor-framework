// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <string>

namespace caf::net {

/// Values for representing bitmask of I/O operations.
enum class operation {
  none = 0x00,
  read = 0x01,
  write = 0x02,
  read_write = 0x03,
  shutdown = 0x04,
};

constexpr operation operator|(operation x, operation y) {
  return static_cast<operation>(static_cast<int>(x) | static_cast<int>(y));
}

constexpr operation operator&(operation x, operation y) {
  return static_cast<operation>(static_cast<int>(x) & static_cast<int>(y));
}

constexpr operation operator^(operation x, operation y) {
  return static_cast<operation>(static_cast<int>(x) ^ static_cast<int>(y));
}

constexpr operation operator~(operation x) {
  return static_cast<operation>(~static_cast<int>(x));
}

std::string to_string(operation x);

} // namespace caf::net
