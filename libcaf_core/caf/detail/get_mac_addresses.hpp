// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"

#include <string>
#include <utility>
#include <vector>

namespace caf::detail {

using iface_info = std::pair<std::string /* interface name */,
                             std::string /* interface address */>;

CAF_CORE_EXPORT std::vector<iface_info> get_mac_addresses();

} // namespace caf::detail
