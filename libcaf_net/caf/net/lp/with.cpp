// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/lp/with.hpp"

#include "caf/net/lp/framing.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/detail/connection_acceptor.hpp"
#include "caf/flow/observable.hpp"
#include "caf/flow/op/mcast.hpp"
#include "caf/internal/accept_handler.hpp"
#include "caf/internal/get_fd.hpp"
#include "caf/internal/lp_flow_bridge.hpp"
#include "caf/internal/make_transport.hpp"
#include "caf/internal/net_config.hpp"

namespace caf::net::lp {

namespace {

template <class Acceptor>
class connection_acceptor_impl : public detail::connection_acceptor {
public:
  using event_type = net::accept_event<frame>;

  connection_acceptor_impl(Acceptor acceptor, size_t max_consecutive_reads,
                           async::producer_resource<event_type> events)
    : acceptor_(std::move(acceptor)),
      max_consecutive_reads_(max_consecutive_reads),
      events_(std::move(events)) {
    // nop
  }

  error start(net::socket_manager* parent) override {
    parent_ = parent;
    mcast_ = parent->add_child(std::in_place_type<flow::op::mcast<event_type>>);
    flow::observable<event_type>{mcast_}.subscribe(events_);
    return none;
  }

  void abort(const error& what) override {
    if (mcast_) {
      mcast_->abort(what);
      mcast_ = nullptr;
    }
  }

  net::socket handle() const override {
    return internal::get_fd(acceptor_);
  }

  expected<net::socket_manager_ptr> try_accept() override {
    if (!mcast_ || !mcast_->has_observers())
      return make_error(sec::runtime_error, "client has disconnected");
    // Accept a new connection.
    auto conn = accept(acceptor_);
    if (!conn)
      return conn.error();
    // Create socket-to-application and application-to-socket buffers.
    auto [s2a_pull, s2a_push] = async::make_spsc_buffer_resource<frame>();
    auto [a2s_pull, a2s_push] = async::make_spsc_buffer_resource<frame>();
    // Push buffers to the client.
    mcast_->push_all(event_type{std::move(s2a_pull), std::move(a2s_push)});
    // Create the flow bridge.
    auto bridge = internal::make_lp_flow_bridge(std::move(a2s_pull),
                                                std::move(s2a_push));
    // Create the socket manager.
    auto transport = internal::make_transport(std::move(*conn),
                                              framing::make(std::move(bridge)));
    transport->max_consecutive_reads(max_consecutive_reads_);
    transport->active_policy().accept();
    return net::socket_manager::make(parent_->mpx_ptr(), std::move(transport));
  }

private:
  net::socket_manager* parent_ = nullptr;

  Acceptor acceptor_;

  size_t max_consecutive_reads_;

  intrusive_ptr<flow::op::mcast<event_type>> mcast_;

  async::producer_resource<event_type> events_;
};

} // namespace

class with_t::config_impl : public internal::net_config {
public:
  using super = internal::net_config;

  using super::super;

  template <class Acceptor>
  expected<disposable> do_start_server(Acceptor& acc) {
    using impl_t = connection_acceptor_impl<Acceptor>;
    auto conn_acc = std::make_unique<impl_t>(std::move(acc),
                                             max_consecutive_reads,
                                             std::move(server_push));
    auto handler = internal::make_accept_handler(std::move(conn_acc),
                                                 max_connections,
                                                 std::move(monitored_actors));
    auto ptr = net::socket_manager::make(mpx, std::move(handler));
    if (mpx->start(ptr))
      return expected<disposable>{disposable{std::move(ptr)}};
    return make_error(sec::logic_error,
                      "failed to register socket manager to net::multiplexer");
  }

  expected<disposable> start_server_impl(net::ssl::tcp_acceptor& acc) override {
    return do_start_server(acc);
  }

  expected<disposable> start_server_impl(net::tcp_accept_socket acc) override {
    return do_start_server(acc);
  }

  template <typename Connection>
  expected<disposable> do_start_client(Connection& conn) {
    auto bridge = internal::make_lp_flow_bridge(std::move(client_pull),
                                                std::move(client_push));
    auto impl = framing::make(std::move(bridge));
    auto transport = internal::make_transport(std::move(conn), std::move(impl));
    transport->active_policy().connect();
    auto ptr = socket_manager::make(mpx, std::move(transport));
    if (mpx->start(ptr))
      return expected<disposable>{disposable{std::move(ptr)}};
    return make_error(sec::logic_error,
                      "failed to register socket manager to net::multiplexer");
  }

  expected<disposable> start_client_impl(net::ssl::connection& conn) override {
    return do_start_client(conn);
  }

  expected<disposable> start_client_impl(net::stream_socket conn) override {
    return do_start_client(conn);
  }

  expected<disposable> start_client_impl(uri&) override {
    // Connecting via URI is not supported in the `with` interface.
    CAF_CRITICAL("Unreachable");
  }

  // State for servers.

  /// Stores the producer resource for `do_start_server`.
  server::push_t server_push;

  // State for clients.

  /// Stores the producer resource for `do_start_client`.
  client::push_t client_push;

  /// Stores the consumer resource for `do_start_client`.
  client::pull_t client_pull;
};

// -- server API ---------------------------------------------------------------

with_t::server::server(config_ptr&& cfg) noexcept : config_(std::move(cfg)) {
  // nop
}

with_t::server::~server() {
  // nop
}

with_t::server&& with_t::server::max_connections(size_t value) && {
  config_->max_connections = value;
  return std::move(*this);
}

with_t::server&& with_t::server::reuse_address(bool value) && {
  if (auto* lazy = std::get_if<internal::net_config::server_config::lazy>(
        &config_->server.value))
    lazy->reuse_addr = value;
  return std::move(*this);
}

void with_t::server::do_monitor(strong_actor_ptr ptr) {
  config_->do_monitor(std::move(ptr));
}

expected<disposable> with_t::server::do_start(push_t push) {
  // Handle an error that could've been created by the DSL during server setup.
  if (config_->err) {
    if (config_->on_error)
      (*config_->on_error)(config_->err);
    return config_->err;
  }
  config_->server_push = std::move(push);
  return config_->start_server();
}

// -- client API ---------------------------------------------------------------

with_t::client::client(config_ptr&& cfg) noexcept : config_(std::move(cfg)) {
  // nop
}

with_t::client::~client() {
  // nop
}

with_t::client&& with_t::client::retry_delay(timespan value) && {
  config_->retry_delay = value;
  return std::move(*this);
}

with_t::client&& with_t::client::connection_timeout(timespan value) && {
  config_->connection_timeout = value;
  return std::move(*this);
}

with_t::client&& with_t::client::max_retry_count(size_t value) && {
  config_->max_retry_count = value;
  return std::move(*this);
}

expected<disposable> with_t::client::do_start(pull_t pull, push_t push) {
  // Handle an error that could've been created by the DSL during server setup.
  if (config_->err) {
    if (config_->on_error)
      (*config_->on_error)(config_->err);
    return config_->err;
  }
  config_->client_pull = std::move(pull);
  config_->client_push = std::move(push);
  return config_->start_client();
}

// -- with API -----------------------------------------------------------------

with_t with(multiplexer* mpx) {
  return with_t{mpx};
}

with_t with(actor_system& sys) {
  return with(multiplexer::from(sys));
}

with_t::with_t(multiplexer* mpx) : config_(new config_impl(mpx)) {
  // nop
}

with_t::with_t(with_t&&) noexcept = default;

with_t& with_t::operator=(with_t&&) noexcept = default;

with_t::~with_t() {
  // nop
}

with_t&& with_t::context(ssl::context ctx) && {
  config_->ctx = std::make_shared<ssl::context>(std::move(ctx));
  return std::move(*this);
}

with_t&& with_t::context(expected<ssl::context> ctx) && {
  if (ctx) {
    config_->ctx = std::make_shared<ssl::context>(std::move(*ctx));
  } else if (ctx.error()) {
    config_->err = std::move(ctx.error());
  }
  return std::move(*this);
}

with_t::server with_t::accept(uint16_t port, std::string bind_address,
                              bool reuse_addr) && {
  config_->server.assign(port, std::move(bind_address), reuse_addr);
  return server{std::move(config_)};
}

with_t::server with_t::accept(tcp_accept_socket fd) && {
  config_->server.assign(std::move(fd));
  return server{std::move(config_)};
}

with_t::server with_t::accept(ssl::tcp_acceptor acc) && {
  config_->ctx = acc.ctx_ptr();
  config_->server.assign(acc.fd());
  return server{std::move(config_)};
}

with_t::client with_t::connect(std::string host, uint16_t port) && {
  config_->client.assign(std::move(host), port);
  return client{std::move(config_)};
}

with_t::client with_t::connect(stream_socket fd) && {
  config_->client.assign(fd);
  return client{std::move(config_)};
}

with_t::client with_t::connect(ssl::connection conn) && {
  config_->client.assign(std::move(conn));
  return client{std::move(config_)};
}

void with_t::set_on_error(on_error_callback ptr) {
  config_->on_error = std::move(ptr);
}

} // namespace caf::net::lp
