#include "caf/net/http/server_factory.hpp"

#include "caf/net/fwd.hpp"
#include "caf/net/ssl/tcp_acceptor.hpp"

namespace caf::detail {

namespace {

class http_request_producer_impl : public atomic_ref_counted,
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

} // namespace

http_request_producer::~http_request_producer() {
  // nop
}

http_request_producer_ptr
make_http_request_producer(async::execution_context_ptr ecp,
                           async::spsc_buffer_ptr<net::http::request> buf) {
  auto ptr = make_counted<http_request_producer_impl>(std::move(ecp), buf);
  buf->set_producer(ptr);
  return ptr;
}

namespace {

template <class Acceptor>
class http_conn_acceptor : public connection_acceptor {
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

} // namespace

connection_acceptor_ptr
make_http_conn_acceptor(net::tcp_accept_socket fd,
                        std::vector<net::http::route_ptr> routes,
                        size_t max_consecutive_reads, size_t max_request_size) {
  using impl_t = http_conn_acceptor<net::tcp_accept_socket>;
  return std::make_unique<impl_t>(std::move(fd), std::move(routes),
                                  max_consecutive_reads, max_request_size);
}

connection_acceptor_ptr
make_http_conn_acceptor(net::ssl::tcp_acceptor acceptor,
                        std::vector<net::http::route_ptr> routes,
                        size_t max_consecutive_reads, size_t max_request_size) {
  using impl_t = http_conn_acceptor<net::ssl::tcp_acceptor>;
  return std::make_unique<impl_t>(std::move(acceptor), std::move(routes),
                                  max_consecutive_reads, max_request_size);
}

} // namespace caf::detail
