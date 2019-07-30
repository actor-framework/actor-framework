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
#include <vector>

#include "caf/net/ip.hpp"
#include "caf/optional.hpp"

namespace caf {
namespace net {

/// Utility class bundling access to network interface names and addresses.
class interfaces {
public:
  /// Returns the host and protocol available for a local server socket
  static std::vector<std::pair<std::string, ip>>
  server_address(uint16_t port, const char* host,
                 optional<ip> preferred = none);
};

} // namespace net
} // namespace caf
