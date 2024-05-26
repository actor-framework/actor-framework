// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/dsl/client_config.hpp"
#include "caf/net/dsl/generic_config.hpp"
#include "caf/net/dsl/server_config.hpp"
#include "caf/net/stream_socket.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/detail/net_export.hpp"

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

  template <class T, class... Args>
  static auto make(dsl::server_config_tag<T>, const base_config<Trait>& from,
                   Args&&... args) {
    auto ptr = super::make_impl(std::in_place_type<server_config>, from,
                                std::in_place_type<T>,
                                std::forward<Args>(args)...);
    ptr->trait = from.trait;
    return ptr;
  }

  Trait trait;
};

} // namespace caf::net::lp
