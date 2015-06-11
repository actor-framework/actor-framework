/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_IO_NETWORK_INTERFACES_HPP
#define CAF_IO_NETWORK_INTERFACES_HPP

#include <map>
#include <vector>
#include <string>
#include <utility>

#include "caf/optional.hpp"

#include "caf/io/network/protocol.hpp"

namespace caf {
namespace io {
namespace network {

// {interface_name => {protocol => address}}
using interfaces_map = std::map<std::string,
                                std::map<protocol,
                                         std::vector<std::string>>>;

/// Utility class bundling access to network interface names and addresses.
class interfaces {
public:
  /// Returns a map listing each interface by its name.
  static interfaces_map list_all(bool include_localhost = true);

  /// Returns all addresses for all devices for all protocols.
  static std::map<protocol, std::vector<std::string>>
  list_addresses(bool include_localhost = true);

  /// Returns all addresses for all devices for given protocol.
  static std::vector<std::string> list_addresses(protocol proc,
                                                 bool include_localhost = true);

  /// Returns a native IPv4 or IPv6 translation of `host`.
  ///*/
  static optional<std::pair<std::string, protocol>>
  native_address(const std::string& host, optional<protocol> preferred = none);
};

} // namespace network
} // namespace io
} // namespace caf

#endif // CAF_IO_NETWORK_INTERFACES_HPP
