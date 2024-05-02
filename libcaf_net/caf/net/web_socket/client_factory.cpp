// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/web_socket/client_factory.hpp"

#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/web_socket/client.hpp"

#include "caf/detail/make_transport.hpp"
#include "caf/detail/ws_flow_bridge.hpp"

namespace caf::net::web_socket {

namespace {

/// Specializes the WebSocket flow bridge for the server side.
class ws_client_flow_bridge
  : public detail::ws_flow_bridge<net::web_socket::upper_layer> {
public:
  using super = detail::ws_flow_bridge<net::web_socket::upper_layer>;

  // We consume the output type of the application.
  using pull_t = async::consumer_resource<frame>;

  // We produce the input type of the application.
  using push_t = async::producer_resource<frame>;

  ws_client_flow_bridge(pull_t pull, push_t push)
    : pull_(std::move(pull)), push_(std::move(push)) {
    // nop
  }

  static std::unique_ptr<ws_client_flow_bridge> make(pull_t pull, push_t push) {
    return std::make_unique<ws_client_flow_bridge>(std::move(pull),
                                                   std::move(push));
  }

  error start(net::web_socket::lower_layer* down_ptr) override {
    super::down_ = down_ptr;
    return super::init(&down_ptr->mpx(), std::move(pull_), std::move(push_));
  }

private:
  pull_t pull_;
  push_t push_;
};

template <class Config, class Conn>
expected<disposable> do_start_impl(Config& cfg, Conn conn,
                                   async::consumer_resource<frame> pull,
                                   async::producer_resource<frame> push) {
  // s2a: socket-to-application (and a2s is the inverse).
  using bridge_t = ws_client_flow_bridge;
  auto bridge = bridge_t::make(std::move(pull), std::move(push));
  auto bridge_ptr = bridge.get();
  auto impl = client::make(std::move(cfg.hs), std::move(bridge));
  auto transport = detail::make_transport(std::move(conn), std::move(impl));
  transport->active_policy().connect();
  auto ptr = socket_manager::make(cfg.mpx, std::move(transport));
  bridge_ptr->self_ref(ptr->as_disposable());
  cfg.mpx->start(ptr);
  return expected<disposable>{disposable{std::move(ptr)}};
}

} // namespace

class client_factory::config_impl : public dsl::client_config_value {
public:
  using super = dsl::client_config_value;

  explicit config_impl(multiplexer* mpx) : super(mpx) {
    hs.endpoint("/");
  }

  handshake hs;
};

client_factory::~client_factory() {
  delete config_;
}

dsl::client_config_value& client_factory::base_config() {
  return *config_;
}

dsl::client_config_value& client_factory::init_config(multiplexer* mpx) {
  config_ = new config_impl(mpx);
  return *config_;
}

expected<void> client_factory::sanity_check() {
  if (config_->hs.has_mandatory_fields()) {
    return {};
  } else {
    auto err = make_error(sec::invalid_argument,
                          "WebSocket handshake lacks mandatory fields");
    config_->call_on_error(err);
    return {std::move(err)};
  }
}

expected<disposable> client_factory::do_start(dsl::client_config::lazy& data,
                                              dsl::server_address& addr,
                                              pull_t pull, push_t push) {
  config_->hs.host(addr.host);
  return detail::tcp_try_connect(std::move(addr.host), addr.port,
                                 data.connection_timeout, data.max_retry_count,
                                 data.retry_delay)
    .and_then(with_ssl_connection_or_socket([this, &pull, &push](auto&& conn) {
      using conn_t = decltype(conn);
      return do_start_impl(*config_, std::forward<conn_t>(conn),
                           std::move(pull), std::move(push));
    }));
}

expected<disposable> client_factory::do_start(dsl::client_config::lazy& data,
                                              const uri& addr, pull_t pull,
                                              push_t push) {
  const auto& auth = addr.authority();
  auto host = auth.host_str();
  auto port = auth.port;
  // Sanity checking.
  if (host.empty()) {
    auto err = make_error(sec::invalid_argument,
                          "URI must provide a valid hostname");
    return do_start(err, std::move(pull), std::move(push));
  }
  // Spin up the server based on the scheme.
  auto use_ssl = false;
  if (addr.scheme() == "ws") {
    if (port == 0)
      port = defaults::net::http_default_port;
  } else if (addr.scheme() == "wss") {
    if (port == 0)
      port = defaults::net::https_default_port;
    use_ssl = true;
  } else {
    auto err = make_error(sec::invalid_argument,
                          "unsupported URI scheme: expected ws or wss");
    return do_start(err, std::move(pull), std::move(push));
  }
  // Fill the handshake with fields from the URI and try to connect.
  config_->hs.host(host);
  config_->hs.endpoint(addr.path_query_fragment());
  return detail::tcp_try_connect(std::move(host), port, data.connection_timeout,
                                 data.max_retry_count, data.retry_delay)
    .and_then(with_ssl_connection_or_socket_select(
      use_ssl, [this, &pull, &push](auto&& conn) {
        using conn_t = decltype(conn);
        return do_start_impl(*config_, std::forward<conn_t>(conn),
                             std::move(pull), std::move(push));
      }));
}

expected<disposable> client_factory::do_start(dsl::client_config::lazy& data,
                                              pull_t pull, push_t push) {
  auto fn = [this, &data, &pull, &push](auto& addr) {
    return do_start(data, addr, std::move(pull), std::move(push));
  };
  return std::visit(fn, data.server);
}

expected<disposable> client_factory::do_start(dsl::client_config::socket& data,
                                              pull_t pull, push_t push) {
  return sanity_check()
    .transform([&data] { return data.take_fd(); })
    .and_then(check_socket)
    .and_then(with_ssl_connection_or_socket([this, &pull, &push](auto&& conn) {
      using conn_t = decltype(conn);
      return do_start_impl(*config_, std::forward<conn_t>(conn),
                           std::move(pull), std::move(push));
    })

    );
}

expected<disposable> client_factory::do_start(dsl::client_config::conn& data,
                                              pull_t pull, push_t push) {
  return sanity_check().and_then([&] { //
    return do_start_impl(*config_, std::move(data.state), std::move(pull),
                         std::move(push));
  });
}

expected<disposable> client_factory::do_start(error& err, pull_t, push_t) {
  config_->call_on_error(err);
  return expected<disposable>{std::move(err)};
}

} // namespace caf::net::web_socket
