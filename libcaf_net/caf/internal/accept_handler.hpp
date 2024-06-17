// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_event_layer.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/async/execution_context.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/connection_acceptor.hpp"
#include "caf/detail/connection_factory.hpp"
#include "caf/log/net.hpp"
#include "caf/settings.hpp"

#include <vector>

namespace caf::internal {

/// Creates an accept handler for a connection acceptor.
std::unique_ptr<net::socket_event_layer>
make_accept_handler(detail::connection_acceptor_ptr ptr, size_t max_connections,
                    std::vector<strong_actor_ptr> monitored_actors = {});

} // namespace caf::internal
