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

#include <cstdint>
#include <functional>

#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/ipv6_address.hpp"
#include "caf/meta/type_name.hpp"

namespace caf {

/// An IP endpoint that contains an ::ipv6_address and a port.
class CAF_CORE_EXPORT ipv6_endpoint
  : detail::comparable<ipv6_endpoint>,
    detail::comparable<ipv6_endpoint, ipv4_endpoint> {
public:
  // -- constructors -----------------------------------------------------------

  ipv6_endpoint(ipv6_address address, uint16_t port);

  ipv6_endpoint(ipv4_address address, uint16_t port);

  ipv6_endpoint() = default;

  ipv6_endpoint(const ipv6_endpoint&) = default;

  ipv6_endpoint& operator=(const ipv6_endpoint&) = default;

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

  /// Compares this endpoint to `x`.
  /// @returns 0 if `*this == x`, a positive value if `*this > x` and a negative
  /// value otherwise.
  long compare(ipv6_endpoint x) const noexcept;

  /// Compares this endpoint to `x`.
  /// @returns 0 if `*this == x`, a positive value if `*this > x` and a negative
  /// value otherwise.
  long compare(ipv4_endpoint x) const noexcept;

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
