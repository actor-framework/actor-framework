// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/octet_stream/client_factory.hpp"

#include "caf/net/checked_socket.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/ssl/transport.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/internal/make_transport.hpp"
#include "caf/internal/octet_stream_flow_bridge.hpp"

namespace caf::net::octet_stream {

namespace {

template <class Config, class Conn>
expected<disposable> do_start_impl(Config& cfg, Conn conn,
                                   async::consumer_resource<std::byte> pull,
                                   async::producer_resource<std::byte> push) {
  auto bridge = internal::make_octet_stream_flow_bridge(cfg.read_buffer_size,
                                                        cfg.write_buffer_size,
                                                        std::move(pull),
                                                        std::move(push));
  auto transport = internal::make_transport(std::move(conn), std::move(bridge));
  transport->active_policy().connect();
  auto ptr = net::socket_manager::make(cfg.mpx, std::move(transport));
  cfg.mpx->start(ptr);
  return disposable{std::move(ptr)};
}

} // namespace

class client_factory::config_impl : public dsl::client_config_value {
public:
  using super = dsl::client_config_value;

  using super::super;

  /// Sets the default buffer size for reading from the network.
  uint32_t read_buffer_size = defaults::net::octet_stream_buffer_size;

  /// Sets the default buffer size for writing to the network.
  uint32_t write_buffer_size = defaults::net::octet_stream_buffer_size;
};

client_factory::client_factory(client_factory&& other) noexcept {
  std::swap(config_, other.config_);
}

client_factory& client_factory::operator=(client_factory&& other) noexcept {
  std::swap(config_, other.config_);
  return *this;
}

client_factory::~client_factory() {
  if (config_ != nullptr)
    config_->deref();
}

client_factory&& client_factory::read_buffer_size(uint32_t new_value) && {
  config_->read_buffer_size = new_value;
  return std::move(*this);
}

client_factory&& client_factory::write_buffer_size(uint32_t new_value) && {
  config_->write_buffer_size = new_value;
  return std::move(*this);
}

dsl::client_config_value& client_factory::base_config() {
  return *config_;
}

dsl::client_config_value& client_factory::init_config(multiplexer* mpx) {
  config_ = new config_impl(mpx);
  return *config_;
}

expected<disposable> client_factory::do_start(dsl::client_config::lazy& data,
                                              dsl::server_address& addr,
                                              pull_t pull, push_t push) {
  return detail::tcp_try_connect(std::move(addr.host), addr.port,
                                 data.connection_timeout, data.max_retry_count,
                                 data.retry_delay)
    .and_then(with_ssl_connection_or_socket([this, &pull, &push](auto&& conn) {
      using conn_t = decltype(conn);
      return do_start_impl(*config_, std::forward<conn_t>(conn),
                           std::move(pull), std::move(push));
    }));
}

expected<disposable> client_factory::do_start(dsl::client_config::lazy&,
                                              const uri&, pull_t pull,
                                              push_t push) {
  auto err = make_error(sec::invalid_argument,
                        "connecting via URI is not supported");
  return do_start(err, std::move(pull), std::move(push));
}

expected<disposable> client_factory::do_start(dsl::client_config::lazy& data,
                                              pull_t pull, push_t push) {
  auto fn = [this, &data, &pull, &push](auto& addr) {
    return this->do_start(data, addr, std::move(pull), std::move(push));
  };
  return std::visit(fn, data.server);
}

expected<disposable> client_factory::do_start(dsl::client_config::socket& data,
                                              pull_t pull, push_t push) {
  return expected{data.take_fd()}
    .and_then(check_socket)
    .and_then(with_ssl_connection_or_socket([this, &pull, &push](auto&& conn) {
      using conn_t = decltype(conn);
      return do_start_impl(*config_, std::forward<conn_t>(conn),
                           std::move(pull), std::move(push));
    }));
}

expected<disposable> client_factory::do_start(dsl::client_config::conn& data,
                                              pull_t pull, push_t push) {
  return do_start_impl(*config_, std::move(data.state), std::move(pull),
                       std::move(push));
}

expected<disposable> client_factory::do_start(error& err, pull_t, push_t) {
  config_->call_on_error(err);
  return expected<disposable>{std::move(err)};
}

} // namespace caf::net::octet_stream
