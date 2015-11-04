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
#include <functional>
#include <initializer_list>

#include "caf/maybe.hpp"

#include "caf/io/network/protocol.hpp"

namespace caf {
namespace io {
namespace network {

// {protocol => address}
using address_listing = std::map<protocol, std::vector<std::string>>;

// {interface_name => {protocol => address}}
using interfaces_map = std::map<std::string, address_listing>;

/// Utility class bundling access to network interface names and addresses.
class interfaces {
public:
  /// Consumes `{interface_name, protocol_type, is_localhost, address}` entries.
  using consumer = std::function<void (const char*, protocol,
                                       bool, const char*)>;

  /// Traverses all network interfaces for given protocols using `f`.
  static void traverse(std::initializer_list<protocol> ps, consumer f);

  /// Traverses all network interfaces using `f`.
  static void traverse(consumer f);

  /// Returns a map listing each interface by its name.
  static interfaces_map list_all(bool include_localhost = true);

  /// Returns all addresses for all devices for all protocols.
  static address_listing list_addresses(bool include_localhost = true);

  /// Returns all addresses for all devices for given protocols.
  static std::vector<std::string>
  list_addresses(std::initializer_list<protocol> procs,
                 bool include_localhost = true);

  /// Returns all addresses for all devices for given protocol.
  static std::vector<std::string> list_addresses(protocol proc,
                                                 bool include_localhost = true);

  /// Returns a native IPv4 or IPv6 translation of `host`.
  static maybe<std::pair<std::string, protocol>>
  native_address(const std::string& host, maybe<protocol> preferred = none);
};

} // namespace network
} // namespace io
} // namespace caf

#endif // CAF_IO_NETWORK_INTERFACES_HPP
