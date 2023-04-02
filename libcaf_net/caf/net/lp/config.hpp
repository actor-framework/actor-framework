// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_control_block.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/net/dsl/generic_config.hpp"
#include "caf/net/dsl/server_config.hpp"
#include "caf/net/stream_socket.hpp"

#include <string>
#include <vector>

namespace caf::net::lp {

/// Configuration for the `with_t` DSL entry point. Refined into a server or
/// client configuration later on.
template <class Trait>
class base_config : public dsl::generic_config_value {
public:
  using trait_type = Trait;

  using super = dsl::generic_config_value;

  using super::super;

  /// Configures the protocol layer.
  Trait trait;
};

/// The configuration for a length-prefix framing server.
template <class Trait>
class server_config : public dsl::server_config_value {
public:
  using trait_type = Trait;

  using super = dsl::server_config_value;

  using super::super;

  static auto make(dsl::server_config::lazy_t, const base_config<Trait>& from,
                   uint16_t port, std::string bind_address) {
    auto res = make_counted<server_config>(from.mpx);
    if (auto* err = std::get_if<error>(&from.data)) {
      res->data = *err;
    } else {
      auto& src_data = std::get<dsl::generic_config::lazy>(from.data);
      res->data = dsl::server_config::lazy{src_data.ctx, port,
                                           std::move(bind_address)};
      res->trait = from.trait;
    }
    return res;
  }

  static auto make(dsl::server_config::socket_t, const base_config<Trait>& from,
                   tcp_accept_socket fd) {
    auto res = make_counted<server_config>(from.mpx);
    if (auto* err = std::get_if<error>(&from.data)) {
      res->data = *err;
      close(fd);
    } else {
      auto& src_data = std::get<dsl::generic_config::lazy>(from.data);
      res->data = dsl::server_config::socket{src_data.ctx, fd};
      res->trait = from.trait;
    }
    return res;
  }

  Trait trait;
};

/// The configuration for a length-prefix framing client.
template <class Trait>
class client_config : public dsl::client_config_value {
public:
  using trait_type = Trait;

  using super = dsl::client_config_value;

  using super::super;

  template <class... Ts>
  static auto
  make(dsl::client_config::lazy_t, const base_config<Trait>& from, Ts&&... args) {
    auto res = make_counted<client_config>(from.mpx);
    if (auto* err = std::get_if<error>(&from.data)) {
      res->data = *err;
    } else {
      auto& src_data = std::get<dsl::generic_config::lazy>(from.data);
      res->data = dsl::client_config::lazy{src_data.ctx,
                                           std::forward<Ts>(args)...};
      res->trait = from.trait;
    }
    return res;
  }

  static auto make(dsl::client_config::socket_t, const base_config<Trait>& from,
                   stream_socket fd) {
    auto res = make_counted<client_config>(from.mpx);
    if (auto* err = std::get_if<error>(&from.data)) {
      res->data = *err;
      close(fd);
    } else {
      auto& src_data = std::get<dsl::generic_config::lazy>(from.data);
      res->data = dsl::client_config::socket{src_data.ctx, fd};
      res->trait = from.trait;
    }
    return res;
  }

  static auto make(dsl::client_config::socket_t, const base_config<Trait>& from,
                   ssl::connection conn) {
    auto res = make_counted<client_config>(from.mpx);
    if (auto* err = std::get_if<error>(&from.data)) {
      res->data = *err;
    } else {
      auto& src_data = std::get<dsl::generic_config::lazy>(from.data);
      res->data = dsl::client_config::socket{src_data.ctx, std::move(conn)};
      res->trait = from.trait;
    }
    return res;
  }

  static auto make(dsl::client_config::fail_t, const base_config<Trait>& from,
                   error err) {
    auto res = make_counted<client_config>(from.mpx);
    res->data = std::move(err);
    return res;
  }

  Trait trait;
};

} // namespace caf::net::lp
