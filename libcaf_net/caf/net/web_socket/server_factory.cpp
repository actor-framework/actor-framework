// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/web_socket/server_factory.hpp"

#include "caf/net/multiplexer.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/web_socket/frame.hpp"
#include "caf/net/web_socket/server.hpp"

#include "caf/detail/assert.hpp"
#include "caf/detail/connection_acceptor.hpp"
#include "caf/internal/accept_handler.hpp"
#include "caf/internal/get_fd.hpp"
#include "caf/internal/make_transport.hpp"
#include "caf/internal/ws_flow_bridge.hpp"

#include <cstdint>

namespace caf::net::web_socket {

namespace {

template <class Acceptor>
class connection_acceptor_impl : public detail::connection_acceptor {
public:
  connection_acceptor_impl(Acceptor acceptor, detail::ws_conn_acceptor_ptr wca,
                           size_t max_consecutive_reads)
    : acceptor_(std::move(acceptor)),
      wca_(std::move(wca)),
      max_consecutive_reads_(max_consecutive_reads) {
    // nop
  }

  error start(net::socket_manager* parent) override {
    parent_ = parent;
    return {};
  }

  void abort(const error& reason) override {
    wca_->abort(reason);
  }

  expected<net::socket_manager_ptr> try_accept() override {
    if (wca_->canceled()) {
      return make_error(sec::runtime_error,
                        "WebSocket connection dropped: client canceled");
    }
    auto conn = accept(acceptor_);
    if (!conn) {
      return conn.error();
    }
    auto app = internal::make_ws_flow_bridge(wca_);
    auto ws = net::web_socket::server::make(std::move(app));
    auto transport = internal::make_transport(std::move(*conn), std::move(ws));
    transport->max_consecutive_reads(max_consecutive_reads_);
    transport->active_policy().accept();
    return net::socket_manager::make(parent_->mpx_ptr(), std::move(transport));
  }

  net::socket handle() const override {
    return internal::get_fd(acceptor_);
  }

private:
  Acceptor acceptor_;
  net::socket_manager* parent_ = nullptr;
  detail::ws_conn_acceptor_ptr wca_;
  size_t max_consecutive_reads_;
};

template <class Config, class Acceptor>
expected<disposable> do_start_impl(Config& cfg, Acceptor acc) {
  using impl_t = connection_acceptor_impl<Acceptor>;
  auto conn_acc = std::make_unique<impl_t>(std::move(acc), cfg.wca,
                                           cfg.max_consecutive_reads);
  auto handler = internal::make_accept_handler(std::move(conn_acc),
                                               cfg.max_connections);
  auto ptr = net::socket_manager::make(cfg.mpx, std::move(handler));
  cfg.mpx->start(ptr);
  return expected<disposable>{disposable{std::move(ptr)}};

  // using transport_t = typename Acceptor::transport_type;
  // using factory_t = ws_flow_conn_factory<transport_t>;
  // using impl_t = detail::accept_handler<Acceptor>;
  // auto factory = std::make_unique<factory_t>(cfg.wca,
  //                                            cfg.max_consecutive_reads);
  // auto impl = impl_t::make(std::move(acc), std::move(factory),
  //                          cfg.max_connections);
  // auto impl_ptr = impl.get();
  // auto ptr = net::socket_manager::make(cfg.mpx, std::move(impl));
  // impl_ptr->self_ref(ptr->as_disposable());
  // cfg.mpx->start(ptr);
  // return expected<disposable>{disposable{std::move(ptr)}};
}

} // namespace

/// The configuration for a WebSocket server.
class server_factory_base::config_impl : public dsl::server_config_value {
public:
  using super = dsl::server_config_value;

  using super::super;

  detail::ws_conn_acceptor_ptr wca;
};

server_factory_base::server_factory_base(config_impl* config,
                                         detail::ws_conn_acceptor_ptr wca)
  : config_(config) {
  config->ref();
  config_->wca = std::move(wca);
}

server_factory_base::server_factory_base(server_factory_base&& other) noexcept {
  std::swap(config_, other.config_);
}

server_factory_base&
server_factory_base::operator=(server_factory_base&& other) noexcept {
  std::swap(config_, other.config_);
  return *this;
}

server_factory_base::~server_factory_base() {
  if (config_ != nullptr)
    config_->deref();
}

server_factory_base::config_impl*
server_factory_base::make_config(multiplexer* mpx) {
  return new config_impl(mpx);
}

dsl::server_config_value& server_factory_base::upcast(config_impl& cfg) {
  return cfg;
}

void server_factory_base::release(config_impl* ptr) {
  ptr->deref();
}

expected<disposable>
server_factory_base::do_start(dsl::server_config::socket& data) {
  return checked_socket(data.take_fd())
    .and_then(config_->with_ssl_acceptor_or_socket([this](auto&& acc) {
      using acc_t = decltype(acc);
      return do_start_impl(*config_, std::forward<acc_t>(acc));
    }));
}

expected<disposable>
server_factory_base::do_start(dsl::server_config::lazy& data) {
  return make_tcp_accept_socket(data.port, data.bind_address, data.reuse_addr)
    .and_then(config_->with_ssl_acceptor_or_socket([this](auto&& acc) {
      using acc_t = decltype(acc);
      return do_start_impl(*config_, std::forward<acc_t>(acc));
    }));
}

expected<disposable> server_factory_base::do_start(error& err) {
  return expected<disposable>{std::move(err)};
}

} // namespace caf::net::web_socket
