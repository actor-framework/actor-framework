// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/io_export.hpp"
#include "caf/detail/io_network_deprecated.hpp"

#include <cstddef>
#include <string>

namespace caf::io::network {

/// Bundles protocol information for network and transport layer communication.
struct CAF_IO_NETWORK_DEPRECATED_CLASS protocol {
  /// Denotes a network protocol, i.e., IPv4 or IPv6.
  enum network { ipv4, ipv6 };
  /// Denotes a transport protocol, i.e., TCP or UDP.
  enum transport { tcp, udp };
  transport trans;
  network net;
};

CAF_IO_NETWORK_DEPRECATED
constexpr bool operator==(const protocol& lhs, const protocol& rhs) noexcept {
  return lhs.trans == rhs.trans && lhs.net == rhs.net;
}

CAF_IO_NETWORK_DEPRECATED
constexpr bool operator!=(const protocol& lhs, const protocol& rhs) noexcept {
  return !(lhs == rhs);
}

CAF_IO_NETWORK_DEPRECATED
inline std::string to_string(protocol::transport x) {
  return x == protocol::tcp ? "TCP" : "UDP";
}

template <class Inspector>
CAF_IO_NETWORK_DEPRECATED bool inspect(Inspector& f, protocol::transport& x) {
  using integer_type = std::underlying_type_t<protocol::transport>;
  auto get_as_integer = [&x] { return static_cast<integer_type>(x); };
  auto set = [&x](integer_type val) {
    x = static_cast<protocol::transport>(val);
    return true;
  };
  return f.apply(get_as_integer, set);
}

CAF_IO_NETWORK_DEPRECATED
inline std::string to_string(protocol::network x) {
  return x == protocol::ipv4 ? "IPv4" : "IPv6";
}

template <class Inspector>
CAF_IO_NETWORK_DEPRECATED bool inspect(Inspector& f, protocol::network& x) {
  using integer_type = std::underlying_type_t<protocol::network>;
  auto get_as_integer = [&x] { return static_cast<integer_type>(x); };
  auto set = [&x](integer_type val) {
    x = static_cast<protocol::network>(val);
    return true;
  };
  return f.apply(get_as_integer, set);
}

template <class Inspector>
CAF_IO_NETWORK_DEPRECATED bool inspect(Inspector& f, protocol& x) {
  return f.object(x).fields(f.field("trans", x.trans), f.field("net", x.net));
}

/// Converts a protocol into a transport/network string representation, e.g.,
/// "TCP/IPv4".
CAF_IO_NETWORK_DEPRECATED
CAF_IO_EXPORT std::string to_string(const protocol& x);

} // namespace caf::io::network
