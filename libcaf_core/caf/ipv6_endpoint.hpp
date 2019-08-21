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

#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "caf/ipv6_address.hpp"
#include "caf/meta/type_name.hpp"

namespace caf {

/// A hashable endpoint abstraction for ipv6.
struct ipv6_endpoint : detail::comparable<ipv6_endpoint> {
public:
  // -- constructors -----------------------------------------------------------

  ipv6_endpoint(ipv6_address address, uint16_t port);

  ipv6_endpoint() = default;

  ipv6_endpoint(const ipv6_endpoint&) = default;

  ipv6_endpoint& operator=(const ipv6_endpoint&) = default;

  // -- properties -------------------------------------------------------------

  /// Returns the IPv6 address.
  ipv6_address address() const noexcept;

  /// Sets the address of this endpoint.
  void address(ipv6_address x) noexcept;

  /// Returns the port of this endpoint.
  uint16_t port() const noexcept;

  /// Sets the port of this endpoint.
  void port(uint16_t x) noexcept;

  /// Returns a hash for this object.
  size_t hash_code() const noexcept;

  /// compares This endpoint to another.
  /// Returns 0 if equal, otherwise >0 if this > x and <0 if this < x.
  long compare(ipv6_endpoint x) const noexcept;

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f,
                                                 ipv6_endpoint& x) {
    return f(meta::type_name("ipv6_endpoint"), x.address_, x.port_);
  }

private:
  ipv6_address address_; /// The address of this endpoint.
  uint16_t port_;        /// The port of this endpoint.
};

std::string to_string(const ipv6_endpoint& ep);

} // namespace caf

namespace std {

template <>
struct hash<caf::ipv6_endpoint> {
  size_t operator()(const caf::ipv6_endpoint& ep) const noexcept {
    return ep.hash_code();
  }
};

} // namespace std
