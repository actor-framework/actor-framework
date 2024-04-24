// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/octet_stream/client_factory.hpp"

#include "caf/net/checked_socket.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/ssl/transport.hpp"
#include "caf/net/tcp_stream_socket.hpp"

namespace caf::net::octet_stream {

namespace {

template <class Config, class Init, class Conn>
expected<disposable> do_start_impl(Config& cfg, Init& initializer, Conn conn) {
  auto bridge = make_flow_bridge(cfg.read_buffer_size, cfg.write_buffer_size,
                                 std::move(initializer));
  auto transport = std::unique_ptr<octet_stream::transport>{};
  if constexpr (std::is_same_v<Conn, ssl::connection>)
    transport = ssl::transport::make(std::move(conn), std::move(bridge));
  else
    transport = octet_stream::transport::make(std::move(conn),
                                              std::move(bridge));
  transport->active_policy().connect();
  auto ptr = net::socket_manager::make(cfg.mpx, std::move(transport));
  cfg.mpx->start(ptr);
  return disposable{std::move(ptr)};
}

} // namespace

expected<disposable> client_factory::do_start(dsl::client_config::lazy& data,
                                              dsl::server_address& addr) {
  return detail::tcp_try_connect(std::move(addr.host), addr.port,
                                 data.connection_timeout, data.max_retry_count,
                                 data.retry_delay)
    .and_then(this->with_ssl_connection_or_socket([this](auto&& conn) {
      using conn_t = decltype(conn);
      return do_start_impl(config(), initializer_, std::forward<conn_t>(conn));
    }));
}

expected<disposable> client_factory::do_start(dsl::client_config::lazy&,
                                              const uri&) {
  auto err = make_error(sec::invalid_argument,
                        "connecting via URI is not supported");
  return do_start(err);
}

expected<disposable> client_factory::do_start(dsl::client_config::lazy& data) {
  auto fn = [this, &data](auto& addr) { return this->do_start(data, addr); };
  return std::visit(fn, data.server);
}

expected<disposable>
client_factory::do_start(dsl::client_config::socket& data) {
  return expected{data.take_fd()}
    .and_then(check_socket)
    .and_then(this->with_ssl_connection_or_socket([this](auto&& conn) {
      using conn_t = decltype(conn);
      return do_start_impl(config(), initializer_, std::forward<conn_t>(conn));
    })

    );
}

expected<disposable> client_factory::do_start(dsl::client_config::conn& data) {
  return do_start_impl(config(), initializer_, std::move(data.state));
}

expected<disposable> client_factory::do_start(error& err) {
  config().call_on_error(err);
  return expected<disposable>{std::move(err)};
}

} // namespace caf::net::octet_stream
