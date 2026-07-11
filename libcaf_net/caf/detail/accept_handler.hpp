// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/socket_event_layer.hpp"

#include "caf/detail/connection_acceptor.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"

#include <memory>
#include <vector>

namespace caf::detail {

/// Creates an accept handler for a connection acceptor.
CAF_NET_EXPORT std::unique_ptr<net::socket_event_layer>
make_accept_handler(connection_acceptor_ptr ptr, size_t max_connections,
                    std::vector<strong_actor_ptr> monitored_actors = {});

} // namespace caf::detail
