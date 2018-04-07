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

#include <map>
#include <vector>
#include <string>
#include <utility>
#include <functional>
#include <initializer_list>

#include "caf/optional.hpp"

#include "caf/io/network/protocol.hpp"
#include "caf/io/network/ip_endpoint.hpp"

namespace caf {
namespace io {
namespace network {

// {protocol => address}
using address_listing = std::map<protocol::network, std::vector<std::string>>;

// {interface_name => {protocol => address}}
using interfaces_map = std::map<std::string, address_listing>;

/// Utility class bundling access to network interface names and addresses.
class interfaces {
public:
  /// Consumes `{interface_name, protocol_type, is_localhost, address}` entries.
  using consumer = std::function<void (const char*, protocol::network,
                                       bool, const char*)>;

  /// Traverses all network interfaces for given protocols using `f`.
  static void traverse(std::initializer_list<protocol::network> ps, consumer f);

  /// Traverses all network interfaces using `f`.
  static void traverse(consumer f);

  /// Returns a map listing each interface by its name.
  static interfaces_map list_all(bool include_localhost = true);

  /// Returns all addresses for all devices for all protocols.
  static address_listing list_addresses(bool include_localhost = true);

  /// Returns all addresses for all devices for given protocols.
  static std::vector<std::string>
  list_addresses(std::initializer_list<protocol::network> procs,
                 bool include_localhost = true);

  /// Returns all addresses for all devices for given protocol.
  static std::vector<std::string> list_addresses(protocol::network proc,
                                                 bool include_localhost = true);

  /// Returns a native IPv4 or IPv6 translation of `host`.
  static optional<std::pair<std::string, protocol::network>>
  native_address(const std::string& host,
                 optional<protocol::network> preferred = none);

  /// Returns the host and protocol available for a local server socket
  static std::vector<std::pair<std::string, protocol::network>>
  server_address(uint16_t port, const char* host,
                 optional<protocol::network> preferred = none);

  /// Writes datagram endpoint info for `host`:`port` into ep
  static bool
  get_endpoint(const std::string& host, uint16_t port, ip_endpoint& ep,
               optional<protocol::network> preferred = none);
};

} // namespace network
} // namespace io
} // namespace caf

