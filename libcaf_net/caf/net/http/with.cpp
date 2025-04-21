// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/http/with.hpp"

#include "caf/net/http/async_client.hpp"
#include "caf/net/http/client.hpp"
#include "caf/net/http/router.hpp"
#include "caf/net/http/server.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/detail/connection_acceptor.hpp"
#include "caf/internal/accept_handler.hpp"
#include "caf/internal/make_transport.hpp"
#include "caf/internal/net_config.hpp"

namespace caf::net::http {

namespace {

class http_request_producer : public async::producer {
public:
  virtual bool push(const net::http::request& item) = 0;
};

using http_request_producer_ptr = intrusive_ptr<http_request_producer>;

class http_request_producer_impl : public detail::atomic_ref_counted,
                                   public http_request_producer {
public:
  using buffer_ptr = async::spsc_buffer_ptr<net::http::request>;

  http_request_producer_impl(async::execution_context_ptr ecp, buffer_ptr buf)
    : ecp_(std::move(ecp)), buf_(std::move(buf)) {
    // nop
  }

  void on_consumer_ready() override {
    // nop
  }

  void on_consumer_cancel() override {
    // nop
  }

  void on_consumer_demand(size_t) override {
    // nop
  }

  void ref_producer() const noexcept override {
    ref();
  }

  void deref_producer() const noexcept override {
    deref();
  }

  bool push(const net::http::request& item) override {
    return buf_->push(item);
  }

private:
  async::execution_context_ptr ecp_;
  buffer_ptr buf_;
};

http_request_producer_ptr
make_http_request_producer(async::execution_context_ptr ecp,
                           async::spsc_buffer_ptr<net::http::request> buf) {
  auto ptr = make_counted<http_request_producer_impl>(std::move(ecp), buf);
  buf->set_producer(ptr);
  return ptr;
}

template <class Acceptor>
class http_conn_acceptor : public detail::connection_acceptor {
public:
  http_conn_acceptor(Acceptor acceptor,
                     std::vector<net::http::route_ptr> routes,
                     size_t max_consecutive_reads, size_t max_request_size)
    : acceptor_(std::move(acceptor)),
      routes_(std::move(routes)),
      max_consecutive_reads_(max_consecutive_reads),
      max_request_size_(max_request_size) {
    // nop
  }

  error start(net::socket_manager* parent) override {
    parent_ = parent;
    return error{};
  }

  void abort(const error&) override {
    // nop
  }

  expected<net::socket_manager_ptr> try_accept() override {
    if (parent_ == nullptr)
      return make_error(sec::runtime_error, "acceptor not started");
    auto conn = accept(acceptor_);
    if (!conn)
      return conn.error();
    auto app = net::http::router::make(routes_);
    auto serv = net::http::server::make(std::move(app));
    serv->max_request_size(max_request_size_);

    auto transport = std::unique_ptr<net::octet_stream::transport>{};
    if constexpr (std::is_same_v<std::decay_t<decltype(*conn)>,
                                 net::ssl::connection>)
      transport = net::ssl::transport::make(std::move(*conn), std::move(serv));
    else
      transport = net::octet_stream::transport::make(std::move(*conn),
                                                     std::move(serv));
    transport->max_consecutive_reads(max_consecutive_reads_);
    transport->active_policy().accept();
    auto* mpx = parent_->mpx_ptr();
    auto res = net::socket_manager::make(mpx, std::move(transport));
    mpx->watch(res->as_disposable());
    return res;
  }

  net::socket handle() const override {
    if constexpr (std::is_convertible_v<Acceptor, net::socket>)
      return acceptor_;
    else
      return acceptor_.fd();
  }

private:
  net::socket_manager* parent_ = nullptr;
  Acceptor acceptor_;
  std::vector<net::http::route_ptr> routes_;
  size_t max_consecutive_reads_;
  size_t max_request_size_;
};

detail::connection_acceptor_ptr
make_http_conn_acceptor(net::tcp_accept_socket fd,
                        std::vector<net::http::route_ptr> routes,
                        size_t max_consecutive_reads, size_t max_request_size) {
  using impl_t = http_conn_acceptor<net::tcp_accept_socket>;
  return std::make_unique<impl_t>(std::move(fd), std::move(routes),
                                  max_consecutive_reads, max_request_size);
}

detail::connection_acceptor_ptr
make_http_conn_acceptor(net::ssl::tcp_acceptor acceptor,
                        std::vector<net::http::route_ptr> routes,
                        size_t max_consecutive_reads, size_t max_request_size) {
  using impl_t = http_conn_acceptor<net::ssl::tcp_acceptor>;
  return std::make_unique<impl_t>(std::move(acceptor), std::move(routes),
                                  max_consecutive_reads, max_request_size);
}

} // namespace

class with_t::config_impl : public internal::net_config {
public:
  using super = internal::net_config;

  using push_t = async::producer_resource<request>;

  using pull_t = async::consumer_resource<request>;

  using super::super;

  template <class Acceptor>
  expected<disposable> do_start_server(Acceptor& acc) {
    if (push) {
      auto producer = make_http_request_producer(mpx, push.try_open());
      auto new_route = make_route([producer](responder& res) {
        if (!producer->push(responder{res}.to_request())) {
          auto err = make_error(sec::runtime_error, "flow disconnected");
          res.router()->abort_and_shutdown(err);
        }
      });
      if (!new_route) {
        return std::move(new_route.error());
      }
      routes.push_back(std::move(*new_route));
    } else if (routes.empty()) {
      return make_error(sec::logic_error,
                        "cannot start an HTTP server without any routes");
    }
    for (auto& ptr : routes)
      ptr->init();
    auto factory = make_http_conn_acceptor(std::move(acc), routes,
                                           max_consecutive_reads,
                                           max_request_size);
    auto impl = internal::make_accept_handler(std::move(factory),
                                              max_connections,
                                              monitored_actors);
    auto ptr = net::socket_manager::make(mpx, std::move(impl));
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
    auto app_t = async_client::make(method, path, fields, payload);
    resp = app_t->get_future();
    auto http_client = http::client::make(std::move(app_t));
    auto transport = internal::make_transport(std::move(conn),
                                              std::move(http_client));
    transport->active_policy().connect();
    auto ptr = socket_manager::make(mpx, std::move(transport));
    if (mpx->start(ptr))
      return disposable{std::move(ptr)};
    return make_error(sec::logic_error,
                      "failed to register socket manager to net::multiplexer");
  }

  expected<disposable> start_client_impl(net::ssl::connection& conn) override {
    return do_start_client(conn);
  }

  expected<disposable> start_client_impl(net::stream_socket conn) override {
    return do_start_client(conn);
  }

  expected<disposable> start_client_impl(uri& endpoint) override {
    auto auth = endpoint.authority();
    auto use_ssl = false;
    // Sanity checking.
    if (auth.host_str().empty())
      return make_error(sec::invalid_argument,
                        "URI must provide a valid hostname");
    if (endpoint.scheme() == "http") {
      if (auth.port == 0)
        auth.port = defaults::net::http_default_port;
    } else if (endpoint.scheme() == "https") {
      if (auth.port == 0)
        auth.port = defaults::net::https_default_port;
      use_ssl = true;
    } else {
      auto err = make_error(sec::invalid_argument,
                            "unsupported URI scheme: expected http or https");
      return err;
    }
    if (use_ssl) {
      if (!ctx) {
        if (auto maybe_ctx = (*context_factory)())
          ctx = std::make_shared<ssl::context>(std::move(*maybe_ctx));
        else
          return maybe_ctx.error();
      }
    }
    auto host = endpoint.authority().host_str();
    auto port = endpoint.authority().port;
    return start_client(host, port);
  }

  // State for servers.

  /// Stores the available routes on the HTTP server.
  std::vector<route_ptr> routes;

  /// Store the maximum request size with 0 meaning "default".
  size_t max_request_size = 0;

  /// Stores the producer resource for `do_start_server`.
  push_t push;

  // State for clients.

  /// Store HTTP method for the request.
  http::method method;

  /// Store payload path for the request.
  const_byte_span payload;

  /// Store HTTP path for the request.
  std::string path;

  /// Store HTTP header fields for the request.
  caf::unordered_flat_map<std::string, std::string> fields;

  /// Stores the response from `do_start_client`.
  async::future<response> resp;
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

with_t::server&& with_t::server::max_request_size(size_t value) && {
  config_->max_request_size = value;
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

void with_t::server::add_route(expected<route_ptr>& new_route) {
  if (config_->err)
    return;
  if (new_route) {
    CAF_ASSERT(*new_route != nullptr);
    config_->routes.push_back(std::move(*new_route));
    return;
  }
  config_->err = std::move(new_route.error());
}

expected<disposable> with_t::server::do_start(push_t push) {
  config_->push = std::move(push);
  // Handle an error that could've been created by the DSL during server setup.
  if (config_->err) {
    if (config_->on_error)
      (*config_->on_error)(config_->err);
    return config_->err;
  }
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

with_t::client&& with_t::client::add_header_field(std::string name,
                                                  std::string value) && {
  config_->fields.insert(std::pair{std::move(name), std::move(value)});
  return std::move(*this);
}

expected<std::pair<async::future<response>, disposable>>
with_t::client::request(http::method method, const_byte_span payload) {
  // Handle an error that could've been created by the DSL during client setup.
  if (config_->err) {
    if (config_->on_error)
      (*config_->on_error)(config_->err);
    return config_->err;
  }
  // Only connecting to an URI is enabled in the 'with' DSL.
  using lazy_t = internal::net_config::client_config::lazy;
  CAF_ASSERT(std::holds_alternative<lazy_t>(config_->client.value));
  auto& lazy = std::get<lazy_t>(config_->client.value);
  CAF_ASSERT(std::holds_alternative<uri>(lazy.server));
  auto& endpoint = std::get<uri>(lazy.server);
  config_->path = endpoint.path_query_fragment();
  config_->method = method;
  config_->payload = payload;
  auto lift = [this](disposable&& disp) {
    return std::pair(std::move(config_->resp), std::move(disp));
  };
  return config_->start_client().transform(lift);
}

expected<std::pair<async::future<response>, disposable>>
with_t::client::request(http::method method, std::string_view payload) {
  return request(method, as_bytes(make_span(payload)));
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

void with_t::set_context_factory(
  unique_callback_ptr<expected<ssl::context>()> fn) {
  config_->context_factory = std::move(fn);
}

} // namespace caf::net::http
