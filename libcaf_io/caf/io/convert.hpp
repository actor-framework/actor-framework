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

#include "caf/ip_endpoint.hpp"

struct addrinfo;
struct sockaddr;
struct sockaddr_storage;

namespace caf {
namespace io {

/// Tries to assign the content of `x` to `y`, using transport protocol `tp`.
/// Succeeds only if `y.sa_family == AF_INET || y.sa_family == AF_INET6`.
bool convert(const sockaddr& src, protocol::transport tp, ip_endpoint& dst);

/// Tries to assign the content of `x` to `y`.
bool convert(const ip_endpoint& src, sockaddr_storage& dst);

} // namespace io
} // namespace caf
