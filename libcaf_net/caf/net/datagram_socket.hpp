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

#include "caf/detail/net_export.hpp"
#include "caf/net/network_socket.hpp"
#include "caf/variant.hpp"

namespace caf::net {

/// A datagram-oriented network communication endpoint.
struct CAF_NET_EXPORT datagram_socket : network_socket {
  using super = network_socket;

  using super::super;
};

/// Enables or disables `SIO_UDP_CONNRESET` error on `x`.
/// @relates datagram_socket
error CAF_NET_EXPORT allow_connreset(datagram_socket x, bool new_value);

/// Converts the result from I/O operation on a ::datagram_socket to either an
/// error code or a integer greater or equal to zero.
/// @relates datagram_socket
variant<size_t, sec> CAF_NET_EXPORT
check_datagram_socket_io_res(std::make_signed<size_t>::type res);

} // namespace caf::net
