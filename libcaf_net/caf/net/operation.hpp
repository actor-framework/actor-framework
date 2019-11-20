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

namespace caf::net {

/// Values for representing bitmask of I/O operations.
enum class operation {
  none = 0x00,
  read = 0x01,
  write = 0x02,
  read_write = 0x03,
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
