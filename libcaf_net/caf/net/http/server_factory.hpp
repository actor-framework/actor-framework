// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/blocking_producer.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/accept_handler.hpp"
#include "caf/detail/connection_factory.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/net/checked_socket.hpp"
#include "caf/net/dsl/server_factory_base.hpp"
#include "caf/net/http/request.hpp"
#include "caf/net/http/server.hpp"
#include "caf/net/http/upper_layer.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/ssl/transport.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/none.hpp"

#include <cstdint>
#include <memory>

namespace caf::detail {

class CAF_NET_EXPORT http_request_producer : public atomic_ref_counted,
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

  void on_consumer_ready() override;

  void on_consumer_cancel() override;

  void on_consumer_demand(size_t) override;

  void ref_producer() const noexcept override;

  void deref_producer() const noexcept override;

  bool push(const net::http::request& item);

private:
  buffer_ptr buf_;
};

using http_request_producer_ptr = intrusive_ptr<http_request_producer>;

class CAF_NET_EXPORT http_flow_adapter : public net::http::upper_layer {
public:
  explicit http_flow_adapter(async::execution_context_ptr loop,
                             http_request_producer_ptr ptr)
    : loop_(std::move(loop)), producer_(std::move(ptr)) {
    // nop
  }

  void prepare_send() override;

  bool done_sending() override;

  void abort(const error&) override;

  error start(net::http::lower_layer* down) override;

  ptrdiff_t consume(const net::http::request_header& hdr,
                    const_byte_span payload) override;

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

template <class Transport>
class http_conn_factory
  : public connection_factory<typename Transport::connection_handle> {
public:
  using connection_handle = typename Transport::connection_handle;

  http_conn_factory(http_request_producer_ptr producer,
                    size_t max_consecutive_reads)
    : producer_(std::move(producer)),
      max_consecutive_reads_(max_consecutive_reads) {
    // nop
  }

  net::socket_manager_ptr make(net::multiplexer* mpx,
                               connection_handle conn) override {
    auto app = http_flow_adapter::make(mpx, producer_);
    auto serv = net::http::server::make(std::move(app));
    auto fd = conn.fd();
    auto transport = Transport::make(std::move(conn), std::move(serv));
    transport->max_consecutive_reads(max_consecutive_reads_);
    transport->active_policy().accept(fd);
    auto res = net::socket_manager::make(mpx, std::move(transport));
    mpx->watch(res->as_disposable());
    return res;
  }

private:
  http_request_producer_ptr producer_;
  size_t max_consecutive_reads_;
};

} // namespace caf::detail

namespace caf::net::http {

/// Factory type for the `with(...).accept(...).start(...)` DSL.
class server_factory
  : public dsl::server_factory_base<dsl::config_base, server_factory> {
public:
  using super = dsl::server_factory_base<dsl::config_base, server_factory>;

  using config_type = typename super::config_type;

  using super::super;

  /// Starts a server that accepts incoming connections with the
  /// length-prefixing protocol.
  template <class OnStart>
  [[nodiscard]] expected<disposable> start(OnStart on_start) {
    using consumer_resource = async::consumer_resource<request>;
    static_assert(std::is_invocable_v<OnStart, consumer_resource>);
    auto& cfg = super::config();
    return cfg.visit([this, &cfg, &on_start](auto& data) {
      return this->do_start(cfg, data, on_start)
        .or_else([&cfg](const error& err) { cfg.call_on_error(err); });
    });
  }

private:
  template <class Acceptor, class OnStart>
  expected<disposable>
  do_start_impl(config_type& cfg, Acceptor acc, OnStart& on_start) {
    using transport_t = typename Acceptor::transport_type;
    using factory_t = detail::http_conn_factory<transport_t>;
    using impl_t = detail::accept_handler<Acceptor>;
    auto [pull, push] = async::make_spsc_buffer_resource<request>();
    auto producer = detail::http_request_producer::make(push.try_open());
    auto factory = std::make_unique<factory_t>(std::move(producer),
                                               cfg.max_consecutive_reads);
    auto impl = impl_t::make(std::move(acc), std::move(factory),
                             cfg.max_connections);
    auto impl_ptr = impl.get();
    auto ptr = net::socket_manager::make(cfg.mpx, std::move(impl));
    impl_ptr->self_ref(ptr->as_disposable());
    cfg.mpx->start(ptr);
    on_start(std::move(pull));
    return expected<disposable>{disposable{std::move(ptr)}};
  }

  template <class OnStart>
  expected<disposable> do_start(config_type& cfg,
                                dsl::server_config::socket& data,
                                OnStart& on_start) {
    return checked_socket(data.take_fd())
      .and_then(data.acceptor_with_ctx([this, &cfg, &on_start](auto& acc) {
        return this->do_start_impl(cfg, std::move(acc), on_start);
      }));
  }

  template <class OnStart>
  expected<disposable> do_start(config_type& cfg,
                                dsl::server_config::lazy& data,
                                OnStart& on_start) {
    return make_tcp_accept_socket(data.port, data.bind_address, data.reuse_addr)
      .and_then(data.acceptor_with_ctx([this, &cfg, &on_start](auto& acc) {
        return this->do_start_impl(cfg, std::move(acc), on_start);
      }));
  }

  template <class OnStart>
  expected<disposable> do_start(config_type&, error& err, OnStart&) {
    return expected<disposable>{std::move(err)};
  }
};

} // namespace caf::net::http
