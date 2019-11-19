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

#include <string>
#include <vector>

#include "caf/fwd.hpp"

namespace caf::net::ip {

/// Returns all IP addresses of `host` (if any).
std::vector<ip_address> resolve(string_view host);

/// Returns all IP addresses of `host` (if any).
std::vector<ip_address> resolve(ip_address host);

/// Returns the IP addresses for a local endpoint, which is either an address,
/// an interface name, or the string "localhost".
std::vector<ip_address> local_addresses(string_view host);

/// Returns the IP addresses for a local endpoint address.
std::vector<ip_address> local_addresses(ip_address host);

/// Returns the hostname of this device.
std::string hostname();

} // namespace caf
