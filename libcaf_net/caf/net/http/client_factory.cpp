// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/http/client_factory.hpp"

namespace caf::net::http {

expected<std::pair<async::future<response>, disposable>>
client_factory::request(http::method method, std::string_view payload) {
  using namespace std::literals;
  // Only connecting to an URI is enabled in the 'with' DSL.
  auto& cfg = super::config();
  if (std::holds_alternative<caf::error>(cfg.data))
    return do_start(super::config(), std::get<caf::error>(cfg.data));
  auto& data = std::get<dsl::client_config::lazy>(cfg.data);
  CAF_ASSERT(std::holds_alternative<uri>(data.server));
  const auto& resource = std::get<uri>(data.server);
  cfg.method = method;
  cfg.path = resource.path_query_fragment();
  cfg.payload = payload;
  cfg.fields.emplace("Host"s, resource.authority().host_str());
  return do_start(super::config(), data);
}

client_factory::return_t
client_factory::do_start(config_type& cfg, dsl::client_config::lazy& data) {
  // Only connecting to an URI is enabled in the `with` DSL.
  CAF_ASSERT(std::holds_alternative<uri>(data.server));
  const auto& resource = std::get<uri>(data.server);
  auto auth = resource.authority();
  // Sanity checking.
  if (auth.host_str().empty())
    return do_start(cfg, make_error(sec::invalid_argument,
                                    "URI must provide a valid hostname"));
  if (resource.scheme() == "http") {
    if (auth.port == 0)
      auth.port = defaults::net::http_default_port;
  } else if (resource.scheme() == "https") {
    if (auth.port == 0)
      auth.port = defaults::net::https_default_port;
    // Auto-initialize SSL context for https.
    auto result = data.make_ctx();
    if (!result)
      return do_start(cfg, result.error());
    cfg.ctx = std::move(*result);
    if (!cfg.ctx) {
      auto err = make_error(sec::logic_error,
                            "No SSL context set up for HTTPS schema");
    }
  } else {
    auto err = make_error(sec::invalid_argument,
                          "URI must use http or https scheme");
    return do_start(cfg, err);
  }
  // Try to connect.
  return detail::tcp_try_connect(auth, data.connection_timeout,
                                 data.max_retry_count, data.retry_delay)
    .and_then(connection_with_ctx(cfg.ctx, [this, &cfg](auto& conn) {
      return this->do_start_impl(cfg, std::move(conn));
    }));
}

} // namespace caf::net::http
