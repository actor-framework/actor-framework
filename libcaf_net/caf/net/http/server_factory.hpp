// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/checked_socket.hpp"
#include "caf/net/dsl/generic_config.hpp"
#include "caf/net/dsl/server_factory_base.hpp"
#include "caf/net/http/request.hpp"
#include "caf/net/http/router.hpp"
#include "caf/net/http/server.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/ssl/transport.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/async/blocking_producer.hpp"
#include "caf/async/execution_context.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/accept_handler.hpp"
#include "caf/detail/connection_factory.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/none.hpp"

#include <cstdint>
#include <memory>

namespace caf::net::http {

/// Factory type for the `with(...).accept(...).start(...)` DSL.
class server_factory : public dsl::server_factory_base<server_factory> {
public:
  template <class Token, class... Args>
  server_factory(Token token, const dsl::generic_config_value& from,
                 Args&&... args) {
    init_config(from.mpx).assign(from, token, std::forward<Args>(args)...);
  }

  ~server_factory() override;

  /// Sets the maximum request size to @p value.
  server_factory&& max_request_size(size_t value) &&;

  /// Monitors the actor handle @p hdl and stops the server if the monitored
  /// actor terminates.
  template <class ActorHandle>
  server_factory&& monitor(const ActorHandle& hdl) && {
    do_monitor(actor_cast<strong_actor_ptr>(hdl));
    return std::move(*this);
  }

  /// Adds a new route to the HTTP server.
  /// @param path The path on this server for the new route.
  /// @param f The function object for handling requests on the new route.
  /// @return a reference to `*this`.
  template <class F>
  server_factory&& route(std::string path, F f) && {
    auto new_route = make_route(std::move(path), std::move(f));
    add_route(new_route);
    return std::move(*this);
  }

  /// Adds a new route to the HTTP server.
  /// @param path The path on this server for the new route.
  /// @param method The allowed HTTP method on the new route.
  /// @param f The function object for handling requests on the new route.
  /// @return a reference to `*this`.
  template <class F>
  server_factory&& route(std::string path, http::method method, F f) && {
    auto new_route = make_route(std::move(path), method, std::move(f));
    add_route(new_route);
    return std::move(*this);
  }

  /// Starts a server that makes HTTP requests without a fixed route available
  /// to an observer.
  template <class OnStart>
  [[nodiscard]] expected<disposable> start(OnStart on_start) && {
    static_assert(std::is_invocable_v<OnStart, pull_t>);
    auto [pull, push] = async::make_spsc_buffer_resource<request>();
    auto res = start_impl(std::move(push));
    if (res) {
      on_start(std::move(pull));
    }
    return res;
  }

  /// Starts a server that only serves the fixed routes.
  [[nodiscard]] expected<disposable> start() && {
    return start_impl(push_t{});
  }

protected:
  dsl::server_config_value& base_config() override;

private:
  class config_impl;

  using push_t = async::producer_resource<request>;

  using pull_t = async::consumer_resource<request>;

  dsl::server_config_value& init_config(multiplexer* mpx);

  void do_monitor(strong_actor_ptr ptr);

  void add_route(expected<route_ptr>& new_route);

  expected<disposable> start_impl(push_t push);

  expected<disposable> do_start(dsl::server_config::socket& data, push_t push);

  expected<disposable> do_start(dsl::server_config::lazy& data, push_t push);

  expected<disposable> do_start(error& err, push_t push);

  config_impl* config_ = nullptr;
};

} // namespace caf::net::http
