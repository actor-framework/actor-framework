// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/http/serve.hpp"

#include "caf/detail/accept_handler.hpp"
#include "caf/detail/connection_factory.hpp"
#include "caf/logger.hpp"
#include "caf/net/http/lower_layer.hpp"
#include "caf/net/http/server.hpp"
#include "caf/net/http/upper_layer.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/ssl/acceptor.hpp"
#include "caf/net/ssl/transport.hpp"
#include "caf/net/stream_transport.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/tcp_stream_socket.hpp"

namespace caf::detail {

// TODO: there is currently no back-pressure from the worker to the server.

// -- http_request_producer ----------------------------------------------------

class http_request_producer : public atomic_ref_counted,
                              public async::producer {
public:
  using buffer_ptr = async::spsc_buffer_ptr<net::http::request>;

  http_request_producer(buffer_ptr buf) : buf_(std::move(buf)) {
    // nop
  }

  static auto make(buffer_ptr buf) {
    auto ptr = make_counted<http_request_producer>(buf);
    buf->set_producer(ptr);
    return ptr;
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

  bool push(const net::http::request& item) {
    return buf_->push(item);
  }

private:
  buffer_ptr buf_;
};

using http_request_producer_ptr = intrusive_ptr<http_request_producer>;

// -- http_flow_adapter --------------------------------------------------------

class http_flow_adapter : public net::http::upper_layer {
public:
  explicit http_flow_adapter(async::execution_context_ptr loop,
                             http_request_producer_ptr ptr)
    : loop_(std::move(loop)), producer_(std::move(ptr)) {
    // nop
  }

  void prepare_send() override {
    // nop
  }

  bool done_sending() override {
    return true;
  }

  void abort(const error&) override {
    for (auto& pending : pending_)
      pending.dispose();
  }

  error start(net::http::lower_layer* down, const settings&) override {
    down_ = down;
    down_->request_messages();
    return none;
  }

  ptrdiff_t consume(const net::http::header& hdr,
                    const_byte_span payload) override {
    using namespace net::http;
    if (!pending_.empty()) {
      CAF_LOG_WARNING("received multiple requests from the same HTTP client: "
                      "not implemented yet (drop request)");
      return static_cast<ptrdiff_t>(payload.size());
    }
    auto prom = async::promise<response>();
    auto fut = prom.get_future();
    auto buf = std::vector<std::byte>{payload.begin(), payload.end()};
    auto impl = request::impl{hdr, std::move(buf), std::move(prom)};
    producer_->push(request{std::make_shared<request::impl>(std::move(impl))});
    auto hdl = fut.bind_to(*loop_).then(
      [this](const response& res) {
        down_->begin_header(res.code());
        for (auto& [key, val] : res.header_fields())
          down_->add_header_field(key, val);
        std::ignore = down_->end_header();
        down_->send_payload(res.body());
        down_->shutdown();
      },
      [this](const error& err) {
        auto description = to_string(err);
        down_->send_response(status::internal_server_error, "text/plain",
                             description);
        down_->shutdown();
      });
    pending_.emplace_back(std::move(hdl));
    return static_cast<ptrdiff_t>(payload.size());
  }

  static auto make(async::execution_context_ptr loop,
                   http_request_producer_ptr ptr) {
    return std::make_unique<http_flow_adapter>(loop, ptr);
  }

private:
  async::execution_context_ptr loop_;
  net::http::lower_layer* down_ = nullptr;
  std::vector<disposable> pending_;
  http_request_producer_ptr producer_;
};

// -- http_conn_factory --------------------------------------------------------

template <class Transport>
class http_conn_factory
  : public connection_factory<typename Transport::connection_handle> {
public:
  using connection_handle = typename Transport::connection_handle;

  explicit http_conn_factory(http_request_producer_ptr producer)
    : producer_(std::move(producer)) {
    // nop
  }

  net::socket_manager_ptr make(net::multiplexer* mpx,
                               connection_handle conn) override {
    auto app = http_flow_adapter::make(mpx, producer_);
    auto serv = net::http::server::make(std::move(app));
    auto fd = conn.fd();
    auto transport = Transport::make(std::move(conn), std::move(serv));
    transport->active_policy().accept(fd);
    auto res = net::socket_manager::make(mpx, std::move(transport));
    mpx->watch(res->as_disposable());
    return res;
  }

private:
  http_request_producer_ptr producer_;
};

// -- http_serve_impl ----------------------------------------------------------

template <class Transport, class Acceptor>
disposable http_serve_impl(actor_system& sys, Acceptor acc,
                           async::producer_resource<net::http::request> out,
                           const settings& cfg = {}) {
  using factory_t = http_conn_factory<Transport>;
  using conn_t = typename Transport::connection_handle;
  using impl_t = accept_handler<Acceptor, conn_t>;
  auto max_conn = get_or(cfg, defaults::net::max_connections);
  if (auto buf = out.try_open()) {
    auto& mpx = sys.network_manager().mpx();
    auto producer = http_request_producer::make(std::move(buf));
    auto factory = std::make_unique<factory_t>(std::move(producer));
    auto impl = impl_t::make(std::move(acc), std::move(factory), max_conn);
    auto ptr = net::socket_manager::make(&mpx, std::move(impl));
    mpx.start(ptr);
    return ptr->as_disposable();
  } else {
    return disposable{};
  }
}

} // namespace caf::detail

namespace caf::net::http {

disposable serve(actor_system& sys, tcp_accept_socket fd,
                 async::producer_resource<request> out, const settings& cfg) {
  return detail::http_serve_impl<stream_transport>(sys, fd, std::move(out),
                                                   cfg);
}

disposable serve(actor_system& sys, ssl::acceptor acc,
                 async::producer_resource<request> out, const settings& cfg) {
  return detail::http_serve_impl<ssl::transport>(sys, std::move(acc),
                                                 std::move(out), cfg);
}

} // namespace caf::net::http
