// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/http/sync_client_factory.hpp"

#include "caf/net/http/client.hpp"
#include "caf/net/http/method.hpp"
#include "caf/net/http/response.hpp"
#include "caf/net/http/response_header.hpp"
#include "caf/net/http/upper_layer.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/actor_system.hpp"
#include "caf/async/promise.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/actor_system_access.hpp"
#include "caf/fwd.hpp"
#include "caf/internal/make_transport.hpp"
#include "caf/make_counted.hpp"
#include "caf/ref_counted.hpp"
#include "caf/scheduler.hpp"
#include "caf/sec.hpp"
#include "caf/unordered_flat_map.hpp"

#include <optional>
#include <thread>
#include <utility>
#include <variant>

using namespace std::literals;

namespace caf::net::http {

class sync_client_config : public ref_counted {
public:
  explicit sync_client_config(actor_system& owner) : sys(&owner) {
    // nop
  }

  actor_system* sys;
  caf::unordered_flat_map<std::string, std::string> fields;
  uri endpoint;
  size_t max_response_size = defaults::net::http_max_response_size;
  size_t max_retry_count = 0;
  timespan retry_delay = timespan{1'000'000};
  timespan connection_timeout = infinite;
  unique_callback_ptr<expected<ssl::context>()> context_factory;
  std::unique_ptr<detail::connector> connector;
  error err;
};

void intrusive_ptr_add_ref(const sync_client_config* ptr) noexcept {
  ptr->ref();
}

void intrusive_ptr_release(const sync_client_config* ptr) noexcept {
  ptr->deref();
}

namespace {

std::string host_header_value(const uri& endpoint) {
  const auto& scheme = endpoint.scheme();
  auto auth = endpoint.authority();
  if (auth.userinfo) {
    auth.userinfo = std::nullopt;
  }
  if (scheme == "http" && auth.port == defaults::net::http_default_port) {
    auth.port = 0;
  }
  if (scheme == "https" && auth.port == defaults::net::https_default_port) {
    auth.port = 0;
  }
  return to_string(auth);
}

/// A simple upper layer implementation for fetching a single HTTP response from
/// a server. Meant to run on its own multiplexer and shuts down the multiplexer
/// in its destructor.
class upper_layer_impl : public http::upper_layer::client {
public:
  using result_t = std::variant<none_t, http::response, error>;

  using result_ptr = std::shared_ptr<result_t>;

  result_ptr result = std::make_shared<result_t>(none);

  upper_layer_impl(const_sync_client_config_ptr config, http::method method,
                   byte_buffer payload)
    : config_(std::move(config)),
      method_(method),
      payload_(std::move(payload)) {
    // nop
  }

  ~upper_layer_impl() override {
    if (down) {
      down->mpx().shutdown();
    }
  }

  void prepare_send() override {
    // nop
  }

  bool done_sending() override {
    return true;
  }

  void abort(const caf::error& reason) override {
    if (std::holds_alternative<none_t>(*result)) {
      *result = reason;
    }
  }

  caf::error start(http::lower_layer::client* ll) override {
    down = ll;
    return send_request();
  }

  ptrdiff_t consume(const http::response_header& hdr,
                    caf::const_byte_span payload) override {
    if (!holds_alternative<none_t>(*result)) {
      down->shutdown();
      return static_cast<ptrdiff_t>(payload.size());
    }
    http::response::fields_map fields;
    hdr.for_each_field([&fields](auto key, auto value) {
      fields.container().emplace_back(key, value);
    });
    http::response resp{static_cast<http::status>(hdr.status()),
                        std::move(fields),
                        byte_buffer{payload.begin(), payload.end()}};
    down->shutdown();
    *result = std::move(resp);
    return static_cast<ptrdiff_t>(payload.size());
  }

private:
  caf::error send_request() {
    auto path = config_->endpoint.path_query_fragment();
    if (path.empty()) {
      path = "/";
    }
    down->begin_header(method_, path);
    for (const auto& [key, value] : config_->fields) {
      down->add_header_field(key, value);
    }
    if (!payload_.empty() && !config_->fields.contains("Content-Length")) {
      down->add_header_field("Content-Length", std::to_string(payload_.size()));
    }
    down->end_header();
    if (!payload_.empty()) {
      down->send_payload(payload_);
    }
    // Await response.
    down->request_messages();
    return caf::none;
  }

  http::lower_layer::client* down = nullptr;
  const_sync_client_config_ptr config_;
  http::method method_;
  byte_buffer payload_;
};

sync_client_factory::result_t send_request(const_sync_client_config_ptr config,
                                           http::method method,
                                           byte_buffer payload) {
  using result_t = sync_client_factory::result_t;
  const auto& endpoint = config->endpoint;
  if (!endpoint.valid() || endpoint.scheme().empty()) {
    return result_t{unexpect, sec::invalid_argument,
                    "HTTP client requires a valid URI endpoint"};
  }
  // Get host and port for the TCP connection.
  auto auth = endpoint.authority();
  if (auth.host_str().empty()) {
    return result_t{unexpect, sec::invalid_argument,
                    "URI must provide a valid hostname"};
  }
  if (auth.port == 0) {
    if (endpoint.scheme() == "http")
      auth.port = defaults::net::http_default_port;
    else if (endpoint.scheme() == "https")
      auth.port = defaults::net::https_default_port;
    else {
      return result_t{unexpect, sec::invalid_argument,
                      "unsupported URI scheme: expected http or https"};
    }
  }
  // Create SSL context if needed.
  std::optional<ssl::context> ctx;
  if (endpoint.scheme() == "https") {
    auto make_ctx = [&config] {
      if (config->context_factory) {
        return (*config->context_factory)();
      }
      return net::ssl::context::make_client(net::ssl::tls::v1_2);
    };
    if (auto maybe_ctx = make_ctx()) {
      ctx.emplace(std::move(*maybe_ctx));
    } else {
      return result_t{unexpect, std::move(maybe_ctx.error())};
    }
  }
  // Connect to the server.
  auto try_connect = [&config, &auth]() -> expected<stream_socket> {
    if (config->connector) {
      return config->connector->connect(auth.host_str(), auth.port,
                                        config->connection_timeout,
                                        config->max_retry_count,
                                        config->retry_delay);
    }
    auto fd = net::make_connected_tcp_stream_socket(auth,
                                                    config->connection_timeout);
    if (!fd) {
      return caf::unexpected{std::move(fd.error())};
    }
    return stream_socket{*fd};
  };
  size_t attempt = 0;
  auto maybe_fd = try_connect();
  while (!maybe_fd && ++attempt <= config->max_retry_count) {
    std::this_thread::sleep_for(config->retry_delay);
    maybe_fd = try_connect();
  }
  if (!maybe_fd) {
    return result_t{unexpect, std::move(maybe_fd.error())};
  }
  // Wrap the socket into an SSL connection if needed.
  std::variant<stream_socket, ssl::connection> conn{*maybe_fd};
  if (ctx) {
    auto maybe_conn = ctx->new_connection(*maybe_fd);
    if (!maybe_conn) {
      return result_t{unexpect, std::move(maybe_conn.error())};
    }
    conn = std::move(*maybe_conn);
  }
  // Launch the actual HTTP request; run everything on this thread.
  auto app = std::make_unique<upper_layer_impl>(config, method,
                                                std::move(payload));
  auto res = app->result;
  auto parse = net::http::client::make(std::move(app));
  parse->max_response_size(config->max_response_size);
  auto make_transport = [&conn, &parse] {
    if (std::holds_alternative<stream_socket>(conn)) {
      return caf::internal::make_transport(std::get<stream_socket>(conn),
                                           std::move(parse));
    }
    return caf::internal::make_transport(
      std::move(std::get<ssl::connection>(conn)), std::move(parse));
  };
  auto transport = make_transport();
  transport->active_policy().connect();
  auto mpx = multiplexer::make(config->sys);
  if (auto err = mpx->init(); err.valid()) {
    return result_t{unexpect, std::move(err)};
  }
  mpx->set_thread_id();
  auto mgr = socket_manager::make(mpx.get(), std::move(transport));
  if (!mpx->start(mgr)) {
    return result_t{unexpect, sec::logic_error,
                    "failed to start socket manager"};
  }
  mpx->run();
  if (std::holds_alternative<http::response>(*res)) {
    return std::move(std::get<http::response>(*res));
  }
  if (std::holds_alternative<none_t>(*res)) {
    return result_t{unexpect, sec::runtime_error,
                    "HTTP request failed without a response"};
  }
  return result_t{unexpect, std::move(std::get<error>(*res))};
}

} // namespace

sync_client_factory_builder::sync_client_factory_builder(actor_system& sys,
                                                         uri endpoint)
  : config_(make_counted<sync_client_config>(sys)) {
  config_->endpoint = std::move(endpoint);
}

sync_client_factory_builder::sync_client_factory_builder(actor_system& sys,
                                                         expected<uri> endpoint)
  : config_(make_counted<sync_client_config>(sys)) {
  if (endpoint) {
    config_->endpoint = std::move(*endpoint);
  } else {
    config_->err = std::move(endpoint.error());
  }
}

sync_client_factory_builder::~sync_client_factory_builder() noexcept {
  // nop
}

sync_client_factory_builder&&
sync_client_factory_builder::retry_delay(timespan value) && {
  config_->retry_delay = value;
  return std::move(*this);
}

sync_client_factory_builder&&
sync_client_factory_builder::connection_timeout(timespan value) && {
  config_->connection_timeout = value;
  return std::move(*this);
}

sync_client_factory_builder&&
sync_client_factory_builder::max_response_size(size_t value) && {
  config_->max_response_size = value;
  return std::move(*this);
}

sync_client_factory_builder&&
sync_client_factory_builder::max_retry_count(size_t value) && {
  config_->max_retry_count = value;
  return std::move(*this);
}

sync_client_factory_builder&&
sync_client_factory_builder::add_header_field(std::string name,
                                              std::string value) && {
  do_add_header_field(std::move(name), std::move(value));
  return std::move(*this);
}

sync_client_factory_builder&& sync_client_factory_builder::connector(
  std::unique_ptr<detail::connector>&& ptr) && {
  config_->connector = std::move(ptr);
  return std::move(*this);
}

sync_client_factory sync_client_factory_builder::factory() && {
  if (config_->endpoint.valid()) {
    auto host_field = "Host"s;
    if (!config_->fields.contains(host_field)) {
      do_add_header_field(std::move(host_field),
                          host_header_value(config_->endpoint));
    }
  }
  return sync_client_factory{std::move(config_)};
}

void sync_client_factory_builder::do_add_header_field(std::string name,
                                                      std::string value) {
  config_->fields.emplace(std::move(name), std::move(value));
}

void sync_client_factory_builder::set_context_factory(
  unique_callback_ptr<expected<ssl::context>()> fn) {
  config_->context_factory = std::move(fn);
}

sync_client_factory::~sync_client_factory() noexcept {
  // nop
}

sync_client_factory::result_t
sync_client_factory::request_impl(method method, byte_buffer payload) const {
  switch (method) {
    case http::method::get:
      return get();
    case http::method::head:
      return head();
    case http::method::post:
      return post_impl(std::move(payload));
    case http::method::put:
      return put_impl(std::move(payload));
    case http::method::del:
      return del();
    case http::method::connect:
      return connect();
    case http::method::options:
      return options_impl(std::move(payload));
    case http::method::trace:
      return trace_impl(std::move(payload));
  }
  CAF_RAISE_ERROR("unsupported HTTP method");
}

sync_client_factory::result_t sync_client_factory::get() const {
  return send_request(config_, http::method::get, {});
}

sync_client_factory::result_t sync_client_factory::head() const {
  return send_request(config_, http::method::head, {});
}

sync_client_factory::result_t
sync_client_factory::post_impl(byte_buffer payload) const {
  return send_request(config_, http::method::post, std::move(payload));
}

sync_client_factory::result_t
sync_client_factory::put_impl(byte_buffer payload) const {
  return send_request(config_, http::method::put, std::move(payload));
}

sync_client_factory::result_t sync_client_factory::del() const {
  return send_request(config_, http::method::del, {});
}

sync_client_factory::result_t sync_client_factory::connect() const {
  return send_request(config_, http::method::connect, {});
}

sync_client_factory::result_t
sync_client_factory::options_impl(byte_buffer payload) const {
  return send_request(config_, http::method::options, std::move(payload));
}

sync_client_factory::result_t
sync_client_factory::trace_impl(byte_buffer payload) const {
  return send_request(config_, http::method::trace, std::move(payload));
}

} // namespace caf::net::http
