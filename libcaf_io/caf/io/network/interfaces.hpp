// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/io/network/ip_endpoint.hpp"
#include "caf/io/network/protocol.hpp"

#include "caf/detail/io_export.hpp"

#include <functional>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace caf::io::network {

// {protocol => address}
using address_listing = std::map<protocol::network, std::vector<std::string>>;

// {interface_name => {protocol => address}}
using interfaces_map = std::map<std::string, address_listing>;

/// Utility class bundling access to network interface names and addresses.
class CAF_IO_EXPORT interfaces {
public:
  /// Consumes `{interface_name, protocol_type, is_localhost, address}` entries.
  using consumer
    = std::function<void(const char*, protocol::network, bool, const char*)>;

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
  static std::optional<std::pair<std::string, protocol::network>>
  native_address(const std::string& host,
                 std::optional<protocol::network> preferred = std::nullopt);

  /// Returns the host and protocol available for a local server socket
  static std::vector<std::pair<std::string, protocol::network>>
  server_address(uint16_t port, const char* host,
                 std::optional<protocol::network> preferred = std::nullopt);

  /// Writes datagram endpoint info for `host`:`port` into ep
  static bool
  get_endpoint(const std::string& host, uint16_t port, ip_endpoint& ep,
               std::optional<protocol::network> preferred = std::nullopt);
};

} // namespace caf::io::network
