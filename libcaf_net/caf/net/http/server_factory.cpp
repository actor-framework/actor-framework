#include "caf/net/http/server_factory.hpp"

#include "caf/net/fwd.hpp"
#include "caf/net/ssl/tcp_acceptor.hpp"

#include "caf/internal/accept_handler.hpp"

#include <utility>

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

template <class Config, class Acceptor>
expected<disposable> do_start_impl(Config& cfg, Acceptor acc,
                                   async::producer_resource<request> push) {
  auto& routes = cfg.routes;
  if (push) {
    auto producer = make_http_request_producer(cfg.mpx, push.try_open());
    auto new_route = make_route([producer](responder& res) {
      if (!producer->push(responder{res}.to_request())) {
        auto err = make_error(sec::runtime_error, "flow disconnected");
        res.router()->shutdown(err);
      }
    });
    if (!new_route) {
      cfg.fail(new_route.error());
      return std::move(new_route.error());
    }
    routes.push_back(std::move(*new_route));
  } else if (cfg.routes.empty()) {
    return make_error(sec::logic_error,
                      "cannot start an HTTP server without any routes");
  }
  for (auto& ptr : routes)
    ptr->init();
  auto factory = make_http_conn_acceptor(std::move(acc), cfg.routes,
                                         cfg.max_consecutive_reads,
                                         cfg.max_request_size);
  auto impl = internal::make_accept_handler(std::move(factory),
                                            cfg.max_connections,
                                            cfg.monitored_actors);
  auto ptr = net::socket_manager::make(cfg.mpx, std::move(impl));
  cfg.mpx->start(ptr);
  return expected<disposable>{disposable{std::move(ptr)}};
}

} // namespace

class server_factory::config_impl : public dsl::server_config_value {
public:
  using super = dsl::server_config_value;

  using super::super;

  /// Stores the available routes on the HTTP server.
  std::vector<route_ptr> routes;

  /// Store actors that the server should monitor.
  std::vector<strong_actor_ptr> monitored_actors;

  /// Store the maximum request size with 0 meaning "default".
  size_t max_request_size = 0;
};

server_factory::server_factory(server_factory&& other) noexcept {
  std::swap(config_, other.config_);
}

server_factory& server_factory::operator=(server_factory&& other) noexcept {
  std::swap(config_, other.config_);
  return *this;
}

server_factory::~server_factory() {
  if (config_ != nullptr)
    config_->deref();
}

dsl::server_config_value& server_factory::base_config() {
  return *config_;
}

dsl::server_config_value& server_factory::init_config(multiplexer* mpx) {
  config_ = new config_impl(mpx);
  return *config_;
}

server_factory&& server_factory::max_request_size(size_t value) && {
  config_->max_request_size = value;
  return std::move(*this);
}

void server_factory::do_monitor(strong_actor_ptr ptr) {
  if (ptr) {
    config_->monitored_actors.push_back(std::move(ptr));
    return;
  }
  auto err = make_error(sec::logic_error,
                        "cannot monitor an invalid actor handle");
  config_->fail(std::move(err));
}

void server_factory::add_route(expected<route_ptr>& new_route) {
  if (config_->failed())
    return;
  if (new_route) {
    CAF_ASSERT(*new_route != nullptr);
    config_->routes.push_back(std::move(*new_route));
    return;
  }
  config_->fail(std::move(new_route.error()));
}

expected<disposable> server_factory::start_impl(push_t push) {
  return base_config().visit([this, &push](auto& data) {
    return this->do_start(data, std::move(push))
      .or_else([this](const error& err) { base_config().call_on_error(err); });
  });
}

expected<disposable> server_factory::do_start(dsl::server_config::socket& data,
                                              push_t push) {
  return checked_socket(data.take_fd())
    .and_then(this->with_ssl_acceptor_or_socket([this, &push](auto&& acc) {
      using acc_t = std::decay_t<decltype(acc)>;
      return do_start_impl(*config_, std::forward<acc_t>(acc), std::move(push));
    }));
}

expected<disposable> server_factory::do_start(dsl::server_config::lazy& data,
                                              push_t push) {
  return make_tcp_accept_socket(data.port, data.bind_address, data.reuse_addr)
    .and_then(this->with_ssl_acceptor_or_socket([this, &push](auto&& acc) {
      using acc_t = std::decay_t<decltype(acc)>;
      return do_start_impl(*config_, std::forward<acc_t>(acc), std::move(push));
    }));
}

expected<disposable> server_factory::do_start(error& err, push_t) {
  return expected<disposable>{std::move(err)};
}

} // namespace caf::net::http
