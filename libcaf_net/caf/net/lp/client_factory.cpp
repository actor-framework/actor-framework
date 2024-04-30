// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/lp/client_factory.hpp"

#include "caf/net/lp/default_trait.hpp"

#include "caf/detail/lp_flow_bridge.hpp"
#include "caf/detail/make_transport.hpp"

namespace caf::net::lp {

namespace {

/// Specializes the WebSocket flow bridge for the server side.
class lp_client_flow_bridge : public detail::lp_flow_bridge<default_trait> {
public:
  // We consume the output type of the application.
  using pull_t = async::consumer_resource<frame>;

  // We produce the input type of the application.
  using push_t = async::producer_resource<frame>;

  lp_client_flow_bridge(pull_t pull, push_t push)
    : pull_(std::move(pull)), push_(std::move(push)) {
    // nop
  }

  static std::unique_ptr<lp_client_flow_bridge> make(pull_t pull, push_t push) {
    return std::make_unique<lp_client_flow_bridge>(std::move(pull),
                                                   std::move(push));
  }

  void abort(const error& err) override {
    super::abort(err);
    if (push_)
      push_.abort(err);
  }

  error start(net::lp::lower_layer* down_ptr) override {
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
  auto bridge = lp_client_flow_bridge::make(std::move(pull), std::move(push));
  auto bridge_ptr = bridge.get();
  auto impl = framing::make(std::move(bridge));
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

  using super::super;
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

expected<disposable> client_factory::do_start(dsl::client_config::lazy& data,
                                              pull_t pull, push_t push) {
  if (std::holds_alternative<uri>(data.server)) {
    auto err = make_error(sec::invalid_argument,
                          "length-prefix factories do not accept URIs");
    return do_start(err, std::move(pull), std::move(push));
  }
  auto& addr = std::get<dsl::server_address>(data.server);
  return detail::tcp_try_connect(std::move(addr.host), addr.port,
                                 data.connection_timeout, data.max_retry_count,
                                 data.retry_delay)
    .and_then(with_ssl_connection_or_socket([this, &pull, &push](auto&& conn) {
      using conn_t = decltype(conn);
      return do_start_impl(*config_, std::forward<conn_t>(conn),
                           std::move(pull), std::move(push));
    }));
}

expected<disposable> client_factory::do_start(dsl::client_config::socket& data,
                                              pull_t pull, push_t push) {
  return checked_socket(data.take_fd())
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

} // namespace caf::net::lp
