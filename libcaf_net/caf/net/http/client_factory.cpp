// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/http/client_factory.hpp"

#include "caf/net/http/method.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/detail/assert.hpp"

#include <utility>

namespace caf::net::http {

class client_factory::config_impl : public dsl::client_config_value {
public:
  using super = dsl::client_config_value;

  using super::super;

  std::string path;

  caf::unordered_flat_map<std::string, std::string> fields;
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

dsl::client_config_value& client_factory::base_config() {
  return *config_;
}

dsl::client_config_value& client_factory::init_config(multiplexer* mpx) {
  config_ = new config_impl(mpx);
  return *config_;
}

client_factory& client_factory::add_header_field(std::string key,
                                                 std::string value) {
  config_->fields.insert(std::pair{std::move(key), std::move(value)});
  return *this;
}

expected<std::pair<async::future<response>, disposable>> client_factory::get() {
  return request(http::method::get);
}

expected<std::pair<async::future<response>, disposable>>
client_factory::head() {
  return request(http::method::head);
}

expected<std::pair<async::future<response>, disposable>>
client_factory::post(std::string_view payload) {
  return request(http::method::post, payload);
}

expected<std::pair<async::future<response>, disposable>>
client_factory::put(std::string_view payload) {
  return request(http::method::put, payload);
}

expected<std::pair<async::future<response>, disposable>> client_factory::del() {
  return request(http::method::del);
}

expected<std::pair<async::future<response>, disposable>>
client_factory::connect() {
  return request(http::method::connect);
}

expected<std::pair<async::future<response>, disposable>>
client_factory::options(std::string_view payload) {
  return request(http::method::options, payload);
}

expected<std::pair<async::future<response>, disposable>>
client_factory::trace(std::string_view payload) {
  return request(http::method::trace, payload);
}

expected<std::pair<async::future<response>, disposable>>
client_factory::request(http::method method, const_byte_span payload) {
  using namespace std::literals;
  // Only connecting to an URI is enabled in the 'with' DSL.
  if (std::holds_alternative<caf::error>(config_->data))
    return do_start(std::get<caf::error>(config_->data));
  auto& data = std::get<dsl::client_config::lazy>(config_->data);
  CAF_ASSERT(std::holds_alternative<uri>(data.server));
  const auto& resource = std::get<uri>(data.server);
  config_->path = resource.path_query_fragment();
  config_->fields.emplace("Host"s, resource.authority().host_str());
  return do_start(data, method, payload);
}

expected<std::pair<async::future<response>, disposable>>
client_factory::request(http::method method, std::string_view payload) {
  return request(method, as_bytes(make_span(payload)));
}

template <typename Conn>
expected<std::pair<async::future<response>, disposable>>
client_factory::do_start_impl(Conn conn, http::method method,
                              const_byte_span payload) {
  using transport_t = typename Conn::transport_type;
  auto app_t = async_client::make(method, config_->path, config_->fields,
                                  payload);
  auto ret = app_t->get_future();
  auto http_client = http::client::make(std::move(app_t));
  auto transport = transport_t::make(std::move(conn), std::move(http_client));
  transport->active_policy().connect();
  auto ptr = net::socket_manager::make(config_->mpx, std::move(transport));
  config_->mpx->start(ptr);
  return std::pair{std::move(ret), disposable{std::move(ptr)}};
}

expected<std::pair<async::future<response>, disposable>>
client_factory::do_start(dsl::client_config::lazy& data, http::method method,
                         const_byte_span payload) {
  // Only connecting to an URI is enabled in the `with` DSL.
  CAF_ASSERT(std::holds_alternative<uri>(data.server));
  const auto& resource = std::get<uri>(data.server);
  auto auth = resource.authority();
  auto use_ssl = false;
  // Sanity checking.
  if (auth.host_str().empty())
    return do_start(
      make_error(sec::invalid_argument, "URI must provide a valid hostname"));
  if (resource.scheme() == "http") {
    if (auth.port == 0)
      auth.port = defaults::net::http_default_port;
  } else if (resource.scheme() == "https") {
    if (auth.port == 0)
      auth.port = defaults::net::https_default_port;
    use_ssl = true;
  } else {
    auto err = make_error(sec::invalid_argument,
                          "unsupported URI scheme: expected http or https");
    return return_t{std::move(err)};
  }
  return detail::tcp_try_connect(auth, data.connection_timeout,
                                 data.max_retry_count, data.retry_delay)
    .and_then(this->with_ssl_connection_or_socket_select(
      use_ssl, [this, &method, &payload](auto&& conn) {
        using conn_t = std::decay_t<decltype(conn)>;
        return this->do_start_impl(std::forward<conn_t>(conn), method, payload);
      }));
}

client_factory::return_t client_factory::do_start(error err) {
  config_->call_on_error(err);
  return return_t{std::move(err)};
}

} // namespace caf::net::http
