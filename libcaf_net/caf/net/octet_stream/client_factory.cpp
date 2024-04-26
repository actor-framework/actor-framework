// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/octet_stream/client_factory.hpp"

#include "caf/net/checked_socket.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/ssl/transport.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/detail/make_transport.hpp"
#include "caf/detail/octet_stream_flow_bridge.hpp"

namespace caf::net::octet_stream {

namespace {

template <class Conn>
expected<disposable> do_start_impl(client_factory::config_type& cfg, Conn conn,
                                   async::consumer_resource<std::byte> pull,
                                   async::producer_resource<std::byte> push) {
  auto bridge = detail::make_octet_stream_flow_bridge(cfg.read_buffer_size,
                                                      cfg.write_buffer_size,
                                                      std::move(pull),
                                                      std::move(push));
  auto transport = detail::make_transport(std::move(conn), std::move(bridge));
  transport->active_policy().connect();
  auto ptr = net::socket_manager::make(cfg.mpx, std::move(transport));
  cfg.mpx->start(ptr);
  return disposable{std::move(ptr)};
}

} // namespace

expected<disposable> client_factory::do_start(dsl::client_config::lazy& data,
                                              dsl::server_address& addr,
                                              pull_t pull, push_t push) {
  return detail::tcp_try_connect(std::move(addr.host), addr.port,
                                 data.connection_timeout, data.max_retry_count,
                                 data.retry_delay)
    .and_then(with_ssl_connection_or_socket([this, &pull, &push](auto&& conn) {
      using conn_t = decltype(conn);
      return do_start_impl(config(), std::forward<conn_t>(conn),
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
      return do_start_impl(config(), std::forward<conn_t>(conn),
                           std::move(pull), std::move(push));
    }));
}

expected<disposable> client_factory::do_start(dsl::client_config::conn& data,
                                              pull_t pull, push_t push) {
  return do_start_impl(config(), std::move(data.state), std::move(pull),
                       std::move(push));
}

expected<disposable> client_factory::do_start(error& err, pull_t, push_t) {
  config().call_on_error(err);
  return expected<disposable>{std::move(err)};
}

} // namespace caf::net::octet_stream
