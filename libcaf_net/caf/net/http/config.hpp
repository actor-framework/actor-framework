// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_control_block.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/net/dsl/generic_config.hpp"
#include "caf/net/dsl/server_config.hpp"
#include "caf/net/http/route.hpp"
#include "caf/net/tcp_accept_socket.hpp"

#include <string>
#include <vector>

namespace caf::net::http {

/// Configuration for the `with_t` DSL entry point. Refined into a server or
/// client configuration later on.
using base_config = dsl::generic_config_value;

/// Configuration for the server factory.
class CAF_NET_EXPORT server_config : public dsl::server_config_value {
public:
  using super = dsl::server_config_value;

  using super::super;

  static intrusive_ptr<server_config>
  make(dsl::server_config::lazy_t, const base_config& from, uint16_t port,
       std::string bind_address);

  static intrusive_ptr<server_config> make(dsl::server_config::socket_t,
                                           const base_config& from,
                                           tcp_accept_socket fd);

  /// Stores the available routes on the HTTP server.
  std::vector<route_ptr> routes;

  /// Store actors that the server should monitor.
  std::vector<strong_actor_ptr> monitored_actors;
};

} // namespace caf::net::http
