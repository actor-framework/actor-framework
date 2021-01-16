// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstddef>
#include <string>

#include "caf/detail/io_export.hpp"

namespace caf::io::network {

/// Bundles protocol information for network and transport layer communication.
struct protocol {
  /// Denotes a network protocol, i.e., IPv4 or IPv6.
  enum network { ipv4, ipv6 };
  /// Denotes a transport protocol, i.e., TCP or UDP.
  enum transport { tcp, udp };
  transport trans;
  network net;
};

inline std::string to_string(protocol::transport x) {
  return x == protocol::tcp ? "TCP" : "UDP";
}

template <class Inspector>
bool inspect(Inspector& f, protocol::transport& x) {
  using integer_type = std::underlying_type_t<protocol::transport>;
  auto get = [&x] { return static_cast<integer_type>(x); };
  auto set = [&x](integer_type val) {
    x = static_cast<protocol::transport>(val);
    return true;
  };
  return f.apply(get, set);
}

inline std::string to_string(protocol::network x) {
  return x == protocol::ipv4 ? "IPv4" : "IPv6";
}

template <class Inspector>
bool inspect(Inspector& f, protocol::network& x) {
  using integer_type = std::underlying_type_t<protocol::network>;
  auto get = [&x] { return static_cast<integer_type>(x); };
  auto set = [&x](integer_type val) {
    x = static_cast<protocol::network>(val);
    return true;
  };
  return f.apply(get, set);
}

template <class Inspector>
bool inspect(Inspector& f, protocol& x) {
  return f.object(x).fields(f.field("trans", x.trans), f.field("net", x.net));
}

/// Converts a protocol into a transport/network string representation, e.g.,
/// "TCP/IPv4".
CAF_IO_EXPORT std::string to_string(const protocol& x);

} // namespace caf::io::network
