// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/lp/server_factory.hpp"

#include "caf/chunk.hpp"
#include "caf/cow_tuple.hpp"
#include "caf/flow/observable.hpp"
#include "caf/flow/op/mcast.hpp"
#include "caf/internal/accept_handler.hpp"
#include "caf/internal/get_fd.hpp"
#include "caf/internal/lp_flow_bridge.hpp"
#include "caf/internal/make_transport.hpp"

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

template <class Config, class Acceptor>
expected<disposable>
do_start_impl(Config& cfg, Acceptor acc,
              async::producer_resource<accept_event<frame>> push) {
  using impl_t = connection_acceptor_impl<Acceptor>;
  auto conn_acc = std::make_unique<impl_t>(std::move(acc),
                                           cfg.max_consecutive_reads,
                                           std::move(push));
  auto handler = internal::make_accept_handler(std::move(conn_acc),
                                               cfg.max_connections);
  auto ptr = net::socket_manager::make(cfg.mpx, std::move(handler));
  if (cfg.mpx->start(ptr))
    return expected<disposable>{disposable{std::move(ptr)}};
  return make_error(sec::logic_error,
                    "failed to register socket manager to net::multiplexer");
}

} // namespace

/// The configuration for a length-prefix framing server.
class server_factory::config_impl : public dsl::server_config_value {
public:
  using super = dsl::server_config_value;

  using super::super;
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

expected<disposable> server_factory::do_start(dsl::server_config::socket& data,
                                              push_t push) {
  return checked_socket(data.take_fd())
    .and_then(with_ssl_acceptor_or_socket([this, &push](auto&& acc) {
      using acc_t = decltype(acc);
      return do_start_impl(*config_, std::forward<acc_t>(acc), std::move(push));
    }));
}

expected<disposable> server_factory::do_start(dsl::server_config::lazy& data,
                                              push_t push) {
  return make_tcp_accept_socket(data.port, data.bind_address, data.reuse_addr)
    .and_then(with_ssl_acceptor_or_socket([this, &push](auto&& acc) {
      using acc_t = decltype(acc);
      return do_start_impl(*config_, std::forward<acc_t>(acc), std::move(push));
    }));
}

expected<disposable> server_factory::do_start(error& err, push_t) {
  return expected<disposable>{std::move(err)};
}

} // namespace caf::net::lp
