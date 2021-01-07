// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>
#include <functional>

#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/meta/type_name.hpp"

namespace caf {

/// An IP endpoint that contains an ::ipv4_address and a port.
class CAF_CORE_EXPORT ipv4_endpoint : detail::comparable<ipv4_endpoint> {
public:
  // -- constructors -----------------------------------------------------------

  ipv4_endpoint(ipv4_address address, uint16_t port);

  ipv4_endpoint() = default;

  ipv4_endpoint(const ipv4_endpoint&) = default;

  ipv4_endpoint& operator=(const ipv4_endpoint&) = default;

  // -- properties -------------------------------------------------------------

  /// Returns the IPv4 address.
  ipv4_address address() const noexcept {
    return address_;
  }

  /// Sets the address of this endpoint.
  void address(ipv4_address x) noexcept {
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

  /// Returns a hash for this object
  size_t hash_code() const noexcept;

  /// Compares this endpoint to `x`.
  /// @returns 0 if `*this == x`, a positive value if `*this > x` and a negative
  /// value otherwise.
  long compare(ipv4_endpoint x) const noexcept;

  template <class Inspector>
  friend bool inspect(Inspector& f, ipv4_endpoint& x) {
    return f.object(x).fields(f.field("address", x.address_),
                              f.field("port", x.port_));
  }

private:
  ipv4_address address_; /// The address of this endpoint.
  uint16_t port_;        /// The port of this endpoint.
};

CAF_CORE_EXPORT std::string to_string(const ipv4_endpoint& ep);

} // namespace caf

namespace std {

template <>
struct hash<caf::ipv4_endpoint> {
  size_t operator()(const caf::ipv4_endpoint& ep) const noexcept {
    return ep.hash_code();
  }
};

} // namespace std
