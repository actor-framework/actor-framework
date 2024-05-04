// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/lp/server_factory.hpp"

#include "caf/cow_tuple.hpp"

namespace caf::net::lp {

namespace {

/// Specializes @ref connection_factory for the length-prefixing protocol.
template <class Transport>
class lp_connection_factory
  : public detail::connection_factory<typename Transport::connection_handle> {
public:
  using accept_event_t = accept_event<frame>;

  using producer_type = async::blocking_producer<accept_event_t>;

  using shared_producer_type = std::shared_ptr<producer_type>;

  using connection_handle = typename Transport::connection_handle;

  lp_connection_factory(producer_type producer, size_t max_consecutive_reads)
    : producer_(std::make_shared<producer_type>(std::move(producer))),
      max_consecutive_reads_(max_consecutive_reads) {
    // nop
  }

  net::socket_manager_ptr make(net::multiplexer* mpx,
                               connection_handle conn) override {
    auto bridge = make_lp_flow_bridge(producer_);
    auto impl = net::lp::framing::make(std::move(bridge));
    auto transport = Transport::make(std::move(conn), std::move(impl));
    transport->max_consecutive_reads(max_consecutive_reads_);
    transport->active_policy().accept();
    auto mgr = net::socket_manager::make(mpx, std::move(transport));
    return mgr;
  }

private:
  shared_producer_type producer_;
  size_t max_consecutive_reads_;
};

template <class Config, class Acceptor>
expected<disposable>
do_start_impl(Config& cfg, Acceptor acc,
              async::producer_resource<accept_event<frame>> push) {
  using transport_t = typename Acceptor::transport_type;
  using factory_t = lp_connection_factory<transport_t>;
  using impl_t = detail::accept_handler<Acceptor>;
  auto producer = async::make_blocking_producer(push.try_open());
  auto factory = std::make_unique<factory_t>(std::move(producer),
                                             cfg.max_consecutive_reads);
  auto impl = impl_t::make(std::move(acc), std::move(factory),
                           cfg.max_connections);
  auto impl_ptr = impl.get();
  auto ptr = net::socket_manager::make(cfg.mpx, std::move(impl));
  impl_ptr->self_ref(ptr->as_disposable());
  cfg.mpx->start(ptr);
  return expected<disposable>{disposable{std::move(ptr)}};
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
