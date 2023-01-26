// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>
#include <string>

#include "caf/actor_system.hpp"

#include "caf/io/middleman.hpp"

namespace caf::io {

inline expected<group> remote_group(actor_system& sys,
                                    const std::string& group_uri) {
  return sys.middleman().remote_group(group_uri);
}

inline expected<group> remote_group(actor_system& sys,
                                    const std::string& group_identifier,
                                    const std::string& host, uint16_t port) {
  return sys.middleman().remote_group(group_identifier, host, port);
}

} // namespace caf::io
