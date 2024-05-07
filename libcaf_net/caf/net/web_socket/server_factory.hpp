// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/dsl/server_config.hpp"
#include "caf/net/dsl/server_factory_base.hpp"
#include "caf/net/ssl/transport.hpp"
#include "caf/net/web_socket/acceptor.hpp"

#include "caf/async/spsc_buffer.hpp"
#include "caf/detail/connection_factory.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/detail/ws_conn_acceptor.hpp"
#include "caf/disposable.hpp"
#include "caf/expected.hpp"
#include "caf/fwd.hpp"

namespace caf::net::web_socket {

/// Factory type for the `with(...).accept(...).start(...)` DSL.
class CAF_NET_EXPORT server_factory_base {
public:
  friend class has_on_request;

  class config_impl;

  server_factory_base(config_impl* config, detail::ws_conn_acceptor_ptr wca);

  server_factory_base(server_factory_base&& other) noexcept;

  server_factory_base& operator=(server_factory_base&& other) noexcept;

  virtual ~server_factory_base();

protected:
  static config_impl* make_config(multiplexer* mpx);

  static dsl::server_config_value& upcast(config_impl& cfg);

  static void release(config_impl* ptr);

  expected<disposable> do_start(dsl::server_config::socket& data);

  expected<disposable> do_start(dsl::server_config::lazy& data);

  expected<disposable> do_start(error& err);

  config_impl* config_;
};

/// Factory type for the `with(...).accept(...).start(...)` DSL.
template <class... Ts>
class server_factory : public web_socket::server_factory_base,
                       public dsl::server_factory_base<server_factory<Ts...>> {
public:
  using super = web_socket::server_factory_base;

  using accept_event = cow_tuple<async::consumer_resource<frame>,
                                 async::producer_resource<frame>, Ts...>;

  using pull_t = async::consumer_resource<accept_event>;

  server_factory(super::config_impl* config, detail::ws_conn_acceptor_ptr wca,
                 pull_t pull)
    : super(config, std::move(wca)), pull_(std::move(pull)) {
    // nop
  }

  server_factory(server_factory&&) noexcept = default;

  server_factory& operator=(server_factory&&) noexcept = default;

  server_factory(const server_factory&) = delete;

  server_factory& operator=(const server_factory&) = delete;

  /// Starts a server that accepts incoming connections with the WebSocket
  /// protocol.
  template <class OnStart>
  expected<disposable> start(OnStart on_start) && {
    static_assert(std::is_invocable_v<OnStart, pull_t>);
    auto res = this->base_config().visit([this](auto& data) { //
      return this->do_start(data);
    });
    if (res) {
      on_start(std::move(pull_));
    }
    return res;
  }

protected:
  dsl::server_config_value& base_config() override {
    return super::upcast(*config_);
  }

private:
  pull_t pull_;
};

template <class Acceptor>
struct server_factory_oracle;

template <class... Ts>
struct server_factory_oracle<acceptor<Ts...>> {
  using type = server_factory<Ts...>;
};

template <class Acceptor>
using server_factory_t = typename server_factory_oracle<Acceptor>::type;

} // namespace caf::net::web_socket
