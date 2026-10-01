// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/ipv6_address.hpp"

#include <cstdint>
#include <functional>

namespace caf {

/// An IP endpoint that contains an ::ipv6_address and a port.
class CAF_CORE_EXPORT ipv6_endpoint {
public:
  // -- constructors -----------------------------------------------------------

  ipv6_endpoint(ipv6_address address, uint16_t port) noexcept
    : address_(address), port_(port) {
    // nop
  }

  ipv6_endpoint(ipv4_address address, uint16_t port);

  ipv6_endpoint() = default;

  ipv6_endpoint(const ipv6_endpoint&) = default;

  ipv6_endpoint& operator=(const ipv6_endpoint&) = default;

  static ipv6_endpoint from(const ipv4_endpoint& other);

  // -- properties -------------------------------------------------------------

  /// Returns the IPv6 address.
  ipv6_address address() const noexcept {
    return address_;
  }

  /// Sets the address of this endpoint.
  void address(ipv6_address x) noexcept {
    address_ = x;
  }

  /// Returns the port of this endpoint.
  uint16_t port() const noexcept {
    return port_;
  }

  /// Sets the port of this endpoint.
  void port(uint16_t x) noexcept {
    port_ = x;
  }

  /// Returns a hash for this object.
  size_t hash_code() const noexcept;

  auto operator<=>(const ipv6_endpoint& other) const noexcept = default;

  template <class Inspector>
  friend bool inspect(Inspector& f, ipv6_endpoint& x) {
    return f.object(x).fields(f.field("address", x.address_),
                              f.field("port", x.port_));
  }

private:
  /// The address of this endpoint.
  ipv6_address address_;
  /// The port of this endpoint.
  uint16_t port_;
};

inline bool operator==(const ipv6_endpoint& lhs, const ipv4_endpoint& rhs) {
  return lhs == ipv6_endpoint::from(rhs);
}

inline auto operator<=>(const ipv6_endpoint& lhs, const ipv4_endpoint& rhs) {
  return lhs <=> ipv6_endpoint::from(rhs);
}

inline bool operator==(const ipv4_endpoint& lhs, const ipv6_endpoint& rhs) {
  return ipv6_endpoint::from(lhs) == rhs;
}

inline auto operator<=>(const ipv4_endpoint& lhs, const ipv6_endpoint& rhs) {
  return ipv6_endpoint::from(lhs) <=> rhs;
}

CAF_CORE_EXPORT std::string to_string(const ipv6_endpoint& ep);

} // namespace caf

namespace std {

template <>
struct hash<caf::ipv6_endpoint> {
  size_t operator()(const caf::ipv6_endpoint& ep) const noexcept {
    return ep.hash_code();
  }
};

} // namespace std
