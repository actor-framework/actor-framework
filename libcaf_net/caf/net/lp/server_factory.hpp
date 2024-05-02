// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/accept_event.hpp"
#include "caf/net/checked_socket.hpp"
#include "caf/net/dsl/server_factory_base.hpp"
#include "caf/net/http/server.hpp"
#include "caf/net/lp/config.hpp"
#include "caf/net/lp/framing.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/ssl/transport.hpp"
#include "caf/net/tcp_accept_socket.hpp"

#include "caf/async/blocking_producer.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/accept_handler.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/connection_factory.hpp"
#include "caf/detail/lp_flow_bridge.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/none.hpp"

namespace caf::net::lp {

/// Factory type for the `with(...).accept(...).start(...)` DSL.
class CAF_NET_EXPORT server_factory
  : public dsl::server_factory_base<server_factory> {
public:
  template <class Token, class... Args>
  server_factory(Token token, const dsl::generic_config_value& from,
                 Args&&... args) {
    init_config(from.mpx).assign(from, token, std::forward<Args>(args)...);
  }

  ~server_factory() override;

  /// Starts a server that accepts incoming connections with the
  /// length-prefixing protocol.
  template <class OnStart>
  [[nodiscard]] expected<disposable> start(OnStart on_start) && {
    static_assert(std::is_invocable_v<OnStart, pull_t>);
    auto [pull, push] = async::make_spsc_buffer_resource<accept_event<frame>>();
    auto res
      = base_config().visit([this, push = std::move(push)](auto& data) mutable {
          return this->do_start(data, std::move(push));
        });
    if (res) {
      on_start(std::move(pull));
    }
    return res;
  }

protected:
  dsl::server_config_value& base_config() override;

private:
  class config_impl;

  using push_t = async::producer_resource<accept_event<frame>>;

  using pull_t = async::consumer_resource<accept_event<frame>>;

  dsl::server_config_value& init_config(multiplexer* mpx);

  expected<disposable> do_start(dsl::server_config::socket& data, push_t push);

  expected<disposable> do_start(dsl::server_config::lazy& data, push_t push);

  expected<disposable> do_start(error& err, push_t);

  config_impl* config_ = nullptr;
};

} // namespace caf::net::lp
