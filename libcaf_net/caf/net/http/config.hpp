// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/dsl/client_config.hpp"
#include "caf/net/dsl/generic_config.hpp"
#include "caf/net/dsl/server_config.hpp"
#include "caf/net/http/route.hpp"
#include "caf/net/tcp_accept_socket.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/detail/net_export.hpp"

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

  ~server_config() override;

  using super::super;

  template <class T, class... Args>
  static auto
  make(dsl::server_config_tag<T>, const base_config& from, Args&&... args) {
    auto ptr = super::make_impl(std::in_place_type<server_config>, from,
                                std::in_place_type<T>,
                                std::forward<Args>(args)...);
    return ptr;
  }

  /// Stores the available routes on the HTTP server.
  std::vector<route_ptr> routes;

  /// Store actors that the server should monitor.
  std::vector<strong_actor_ptr> monitored_actors;

  /// Store the maximum request size with 0 meaning "default".
  size_t max_request_size = 0;
};

struct CAF_NET_EXPORT client_config
  : public dsl::config_impl<dsl::client_config::lazy> {
  // Not used, still declared since client_factory_base depends on it.
  using trait_type = void;

  using super = dsl::config_impl<dsl::client_config::lazy>;

  using super::super;

  template <class T, class... Args>
  static auto
  make(dsl::client_config_tag<T>, const base_config& from, Args&&... args) {
    auto ptr = super::make_impl(std::in_place_type<client_config>, from,
                                std::in_place_type<T>,
                                std::forward<Args>(args)...);
    return ptr;
  }

  std::string path;

  caf::unordered_flat_map<std::string, std::string> fields;
};

} // namespace caf::net::http
