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

#include "caf/detail/comparable.hpp"
#include "caf/ip_address.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/meta/type_name.hpp"
#include "caf/protocol.hpp"

namespace caf {

class ip_endpoint : detail::comparable<ip_endpoint> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  ip_endpoint();

  ip_endpoint(ip_address addr, uint16_t port, protocol::transport tp);

  ip_endpoint(ipv4_address addr, uint16_t port, protocol::transport tp);

  ip_endpoint(const ip_endpoint&) = default;

  // -- properties -------------------------------------------------------------

  /// Returns the remote IP address.
  void address(ip_address x) noexcept {
    address_ = x;
  }

  /// Returns the remote IP address.
  const ip_address& address() const noexcept {
    return address_;
  }

  /// Returns the remote port of the endpoint.
  uint16_t port() const noexcept {
    return port_;
  }

  /// Returns the transport protocol of the endpoint.
  protocol::transport transport() const noexcept {
    return transport_;
  }

  /// Returns whether this endpoint has an IPv4 address.
  bool is_v4() const noexcept {
    return address_.embeds_v4();
  }

  /// Returns whether this endpoint has an IPv6 address.
  bool is_v6() const noexcept {
    return !is_v4();
  }

  /// Returns whether this endpoint uses TCP.
  bool is_tcp() const noexcept {
    return transport_ == protocol::tcp;
  }

  /// Returns whether this endpoint uses UDP.
  bool is_udp() const noexcept {
    return transport_ == protocol::udp;
  }

  int compare(const ip_endpoint& other) const noexcept;

  // -- inspection -------------------------------------------------------------

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f, ip_endpoint& x) {
    return f(meta::type_name("ip_endpoint"), x.address_, x.port_, x.transport_);
  }

private:
  ip_address address_;
  uint16_t port_;
  protocol::transport transport_;
};

} // namespace caf

namespace std {

template <>
struct hash<caf::ip_endpoint> {
  size_t operator()(const caf::ip_endpoint& x) const noexcept {
  hash<caf::ip_address> f;
  return f(x.address()) ^ (x.port() << 1 | x.transport());
  }
};

} // namespace std
