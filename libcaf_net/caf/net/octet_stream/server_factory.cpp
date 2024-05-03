// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/octet_stream/server_factory.hpp"

#include "caf/net/receive_policy.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/detail/accept_handler.hpp"
#include "caf/detail/get_fd.hpp"
#include "caf/detail/make_transport.hpp"
#include "caf/detail/octet_stream_flow_bridge.hpp"
#include "caf/flow/observable_builder.hpp"

namespace caf::net::octet_stream {

namespace {

template <class Acceptor>
class connection_acceptor_impl : public detail::connection_acceptor {
public:
  using event_type = net::accept_event<std::byte>;

  connection_acceptor_impl(Acceptor acceptor, uint32_t read_buffer_size,
                           uint32_t write_buffer_size,
                           async::producer_resource<event_type> events)
    : acceptor_(std::move(acceptor)),
      read_buffer_size_(read_buffer_size),
      write_buffer_size_(write_buffer_size),
      events_(std::move(events)) {
    // nop
  }

  static auto make(Acceptor acceptor, uint32_t read_buffer_size,
                   uint32_t write_buffer_size,
                   async::producer_resource<event_type> events) {
    using impl_t = connection_acceptor_impl;
    return std::make_unique<impl_t>(std::move(acceptor), read_buffer_size,
                                    write_buffer_size, std::move(events));
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
    return detail::get_fd(acceptor_);
  }

  expected<net::socket_manager_ptr> try_accept() override {
    if (!mcast_ || !mcast_->has_observers())
      return make_error(sec::runtime_error, "client has disconnected");
    // Accept a new connection.
    auto conn = accept(acceptor_);
    if (!conn)
      return conn.error();
    puts("accepted connection");
    // Create socket-to-application and application-to-socket buffers.
    auto [s2a_pull, s2a_push] = async::make_spsc_buffer_resource<std::byte>();
    auto [a2s_pull, a2s_push] = async::make_spsc_buffer_resource<std::byte>();
    // Push buffers to the client.
    mcast_->push_all(event_type{std::move(s2a_pull), std::move(a2s_push)});
    // Create the flow bridge.
    auto bridge = detail::make_octet_stream_flow_bridge(read_buffer_size_,
                                                        write_buffer_size_,
                                                        std::move(a2s_pull),
                                                        std::move(s2a_push));
    // Create the socket manager.
    auto transport = detail::make_transport(std::move(*conn),
                                            std::move(bridge));
    transport->active_policy().accept();
    return net::socket_manager::make(parent_->mpx_ptr(), std::move(transport));
  }

private:
  net::socket_manager* parent_ = nullptr;

  Acceptor acceptor_;

  uint32_t read_buffer_size_;

  uint32_t write_buffer_size_;

  intrusive_ptr<flow::op::mcast<event_type>> mcast_;

  async::producer_resource<event_type> events_;
};

template <class Config, class Acceptor>
expected<disposable>
do_start_impl(Config& cfg, Acceptor acc,
              async::producer_resource<accept_event<std::byte>> push) {
  using impl_t = connection_acceptor_impl<Acceptor>;
  auto conn_acc = impl_t::make(std::move(acc), cfg.read_buffer_size,
                               cfg.write_buffer_size, std::move(push));
  auto handler = detail::make_accept_handler(std::move(conn_acc),
                                             cfg.max_connections,
                                             cfg.monitored_actors);
  auto ptr = net::socket_manager::make(cfg.mpx, std::move(handler));
  cfg.mpx->start(ptr);
  return expected<disposable>{disposable{std::move(ptr)}};
}

} // namespace

class server_factory::config_impl : public dsl::server_config_value {
public:
  using super = dsl::server_config_value;

  using super::super;

  /// Sets the default buffer size for reading from the network.
  uint32_t read_buffer_size = defaults::net::octet_stream_buffer_size;

  /// Sets the default buffer size for writing to the network.
  uint32_t write_buffer_size = defaults::net::octet_stream_buffer_size;

  /// Store actors that the server should monitor.
  std::vector<strong_actor_ptr> monitored_actors;
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

server_factory&& server_factory::read_buffer_size(uint32_t new_value) && {
  config_->read_buffer_size = new_value;
  return std::move(*this);
}

server_factory&& server_factory::write_buffer_size(uint32_t new_value) && {
  config_->write_buffer_size = new_value;
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

} // namespace caf::net::octet_stream
