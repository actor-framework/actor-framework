// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace caf::net::ip {

/// Returns all IP addresses of `host` (if any).
std::vector<ip_address> CAF_NET_EXPORT resolve(std::string_view host);

/// Returns all IP addresses of `host` (if any).
std::vector<ip_address> CAF_NET_EXPORT resolve(ip_address host);

/// Returns the IP addresses for a local endpoint, which is either an address,
/// an interface name, or the string "localhost".
std::vector<ip_address> CAF_NET_EXPORT local_addresses(std::string_view host);

/// Returns the IP addresses for a local endpoint address.
std::vector<ip_address> CAF_NET_EXPORT local_addresses(ip_address host);

/// Returns the hostname of this device.
std::string CAF_NET_EXPORT hostname();

} // namespace caf::net::ip
