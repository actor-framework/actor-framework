#include "caf/net/web_socket/with.hpp"

#include "caf/net/ssl/context.hpp"
#include "caf/net/ssl/tcp_acceptor.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/web_socket/client.hpp"
#include "caf/net/web_socket/handshake.hpp"
#include "caf/net/web_socket/server.hpp"

#include "caf/defaults.hpp"
#include "caf/detail/connection_acceptor.hpp"
#include "caf/detail/ws_conn_acceptor.hpp"
#include "caf/disposable.hpp"
#include "caf/internal/accept_handler.hpp"
#include "caf/internal/get_fd.hpp"
#include "caf/internal/make_transport.hpp"
#include "caf/internal/net_config.hpp"
#include "caf/internal/ws_flow_bridge.hpp"

namespace caf::net::web_socket {

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

class with_t::config_impl : public internal::net_config {
public:
  using super = internal::net_config;

  using pull_t = async::consumer_resource<frame>;

  using push_t = async::producer_resource<frame>;

  using super::super;

  // state for servers

  detail::ws_conn_acceptor_ptr acceptor;

  template <class Acceptor>
  expected<disposable> do_start_server(Acceptor& acc) {
    using impl_t = connection_acceptor_impl<Acceptor>;
    auto conn_acc = std::make_unique<impl_t>(std::move(acc), acceptor,
                                             max_consecutive_reads);
    auto handler = internal::make_accept_handler(std::move(conn_acc),
                                                 max_connections);
    auto ptr = net::socket_manager::make(mpx, std::move(handler));
    mpx->start(ptr);
    return expected<disposable>{disposable{std::move(ptr)}};
  }

  expected<disposable> start_server_impl(net::ssl::tcp_acceptor& acc) override {
    return do_start_server(acc);
  }

  expected<disposable> start_server_impl(net::tcp_accept_socket acc) override {
    return do_start_server(acc);
  }

  // state for clients

  pull_t pull;

  push_t push;

  handshake hs;

  template <class Connection>
  expected<disposable> do_start_client(Connection& conn) {
    auto bridge = internal::make_ws_flow_bridge(std::move(pull),
                                                std::move(push));
    auto impl = web_socket::client::make(std::move(hs), std::move(bridge));
    auto transport = internal::make_transport(std::move(conn), std::move(impl));
    transport->active_policy().connect();
    auto ptr = socket_manager::make(mpx, std::move(transport));
    mpx->start(ptr);
    return expected<disposable>{disposable{std::move(ptr)}};
  }

  expected<disposable> start_client_impl(net::ssl::connection& conn) override {
    return do_start_client(conn);
  }

  expected<disposable> start_client_impl(net::stream_socket conn) override {
    return do_start_client(conn);
  }

  expected<disposable> start_client_impl(uri& endpoint) override {
    const auto& auth = endpoint.authority();
    auto host = auth.host_str();
    auto port = auth.port;
    // Sanity checking.
    if (host.empty()) {
      return make_error(sec::invalid_argument,
                        "URI must provide a valid hostname");
    }
    // Spin up the server based on the scheme.
    if (endpoint.scheme() == "ws") {
      if (ctx) {
        return make_error(sec::invalid_argument,
                          "URI scheme is ws but SSL context is set");
      }
      if (port == 0) {
        port = defaults::net::http_default_port;
      }
    } else if (endpoint.scheme() == "wss") {
      if (!ctx) {
        using ctx_t = ssl::context;
        auto new_ctx = ctx_t::make_client(ssl::tls::v1_2);
        if (!new_ctx) {
          return new_ctx.error();
        }
        ctx = std::make_shared<ctx_t>(std::move(*new_ctx));
      }
      if (port == 0) {
        port = defaults::net::https_default_port;
      }
    } else {
      return make_error(sec::invalid_argument,
                        "unsupported URI scheme: expected ws or wss");
    }
    // Fill the handshake with fields from the URI and try to connect.
    hs.host(host);
    hs.endpoint(endpoint.path_query_fragment());
    return start_client(host, port);
  }
};

// -- server API ---------------------------------------------------------------

with_t::server_launcher_base::server_launcher_base(config_ptr&& cfg) noexcept
  : config_(std::move(cfg)) {
}

with_t::server_launcher_base::~server_launcher_base() {
  // nop
}

expected<disposable> with_t::server_launcher_base::do_start() {
  return config_->start_server();
}

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

void with_t::server::set_acceptor(detail::ws_conn_acceptor_ptr acc) {
  config_->acceptor = std::move(acc);
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

with_t::client&& with_t::client::protocols(std::string value) && {
  config_->hs.protocols(std::move(value));
  return std::move(*this);
}

with_t::client&& with_t::client::extensions(std::string value) && {
  config_->hs.extensions(std::move(value));
  return std::move(*this);
}

with_t::client&& with_t::client::header_field(std::string_view key,
                                              std::string value) && {
  config_->hs.field(key, std::move(value));
  return std::move(*this);
}

expected<disposable> with_t::client::do_start(pull_t& pull, push_t& push) {
  config_->pull = std::move(pull);
  config_->push = std::move(push);
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
  config_->hs.host(host);
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

with_t::client with_t::connect(uri endpoint) && {
  config_->client.assign(std::move(endpoint));
  return client{std::move(config_)};
}

with_t::client with_t::connect(expected<uri> endpoint) && {
  if (endpoint) {
    config_->client.assign(std::move(*endpoint));
  } else if (endpoint.error()) {
    config_->err = std::move(endpoint.error());
  }
  return client{std::move(config_)};
}

void with_t::set_on_error(on_error_callback ptr) {
  config_->on_error = std::move(ptr);
}

} // namespace caf::net::web_socket
