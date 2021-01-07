// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/ipv6_address.hpp"

namespace caf {

/// An IP address. The address family is IPv6 unless `embeds_v4` returns true.
using ip_address = ipv6_address;

} // namespace caf
