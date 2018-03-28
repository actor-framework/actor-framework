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

#include <cstddef>
#include <string>

#include "caf/meta/type_name.hpp"

namespace caf {
namespace io {
namespace network {

/// Bundles protocol information for network and transport layer communication.
struct protocol {
  /// Denotes a network protocol, i.e., IPv4 or IPv6.
  enum network {
    ipv4,
    ipv6
  };
  /// Denotes a transport protocol, i.e., TCP or UDP.
  enum transport {
    tcp,
    udp
  };
  transport trans;
  network net;
};

/// @relates protocol::transport
inline std::string to_string(protocol::transport x) {
  return x == protocol::tcp ? "TCP" : "UDP";
}

/// @relates protocol::network
inline std::string to_string(protocol::network x) {
  return x == protocol::ipv4 ? "IPv4" : "IPv6";
}

/// @relates protocol
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, protocol& x) {
  return f(meta::type_name("protocol"), x.trans, x.net);
}

/// Converts a protocol into a transport/network string representation, e.g.,
/// "TCP/IPv4".
/// @relates protocol
std::string to_string(const protocol& x);

} // namespace network
} // namespace io
} // namespace caf

