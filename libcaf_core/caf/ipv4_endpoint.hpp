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

#include "caf/ipv4_address.hpp"
#include "caf/meta/type_name.hpp"

namespace caf {

/// A hashable endpoint abstraction for ipv4
struct ipv4_endpoint : detail::comparable<ipv4_endpoint> {
public:
  // -- constructors -----------------------------------------------------------

  ipv4_endpoint(ipv4_address address, uint16_t port);

  ipv4_endpoint() = default;

  ipv4_endpoint(const ipv4_endpoint&) = default;

  ipv4_endpoint& operator=(const ipv4_endpoint&) = default;

  // -- properties -------------------------------------------------------------

  /// Returns the IPv6 address.
  ipv4_address address() const noexcept;

  /// Sets the address of this endpoint.
  void address(ipv4_address x) noexcept;

  /// Returns the port of this endpoint.
  uint16_t port() const noexcept;

  /// Sets the port of this endpoint.
  void port(uint16_t x) noexcept;

  /// Returns a hash for this object
  size_t hash_code() const noexcept;

  /// compares This endpoint to another.
  /// Returns 0 if equal, otherwise >0 if this > x and <0 if this < x
  long compare(ipv4_endpoint x) const noexcept;

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f,
                                                 ipv4_endpoint& x) {
    return f(meta::type_name("ipv4_endpoint"), x.address_, x.port_);
  }

private:
  ipv4_address address_; /// The address of this endpoint.
  uint16_t port_;        /// The port of this endpoint.
};

std::string to_string(const ipv4_endpoint& ep);

} // namespace caf

namespace std {

template <>
struct hash<caf::ipv4_endpoint> {
  size_t operator()(const caf::ipv4_endpoint& ep) const noexcept {
    return ep.hash_code();
  }
};

} // namespace std
