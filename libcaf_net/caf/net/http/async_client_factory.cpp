// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/http/async_client_factory.hpp"

#include "caf/net/http/async_client.hpp"
#include "caf/net/http/client.hpp"
#include "caf/net/http/method.hpp"
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
#include <utility>

using namespace std::literals;

namespace caf::net::http {

class async_client_config : public ref_counted {
public:
  explicit async_client_config(actor_system& owner) : sys(&owner) {
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

namespace {

// Builds the RFC 7230 `Host` header value from endpoint. The port is
// omitted when it is unset, or equal to the default.
std::string host_header_value(const uri& endpoint) {
  const auto& scheme = endpoint.scheme();
  auto auth = endpoint.authority();
  if (auth.userinfo) {
    auth.userinfo = std::nullopt; // suppress userinfo in 'Host' header field
  }
  if (scheme == "http" && auth.port == defaults::net::http_default_port) {
    auth.port = 0; // suppress default HTTP port in 'Host' Header field
  }
  if (scheme == "https" && auth.port == defaults::net::https_default_port) {
    auth.port = 0; // suppress default HTTPS port in 'Host' Header field
  }
  return to_string(auth);
}

template <class Connection>
void start_request(const_async_client_config_ptr config, http::method method,
                   byte_buffer payload, caf::async::promise<response> result,
                   Connection conn) {
  auto path = config->endpoint.path_query_fragment();
  if (path.empty()) {
    path = "/";
  }
  auto app = async_client::make(method, std::move(path), config->fields,
                                std::move(payload), result);
  auto parse = net::http::client::make(std::move(app));
  parse->max_response_size(config->max_response_size);
  auto transport = caf::internal::make_transport(std::move(conn),
                                                 std::move(parse));
  transport->active_policy().connect();
  auto* mpx = config->sys->network_manager().mpx_ptr();
  auto ptr = socket_manager::make(mpx, std::move(transport));
  if (!mpx->start(ptr)) {
    auto what = make_error(sec::logic_error, "failed to start socket manager");
    result.set_error(std::move(what));
    return;
  }
  if (!result.on_dispose(nullptr, make_single_shot_action(
                                    [mgr = ptr] { mgr->dispose(); }))) {
    ptr->dispose();
  }
}

class request_starter : public resumable {
public:
  request_starter(const_async_client_config_ptr config, http::method method,
                  byte_buffer payload, caf::async::promise<response> promise)
    : config_(std::move(config)),
      method_(method),
      payload_(std::move(payload)),
      result_(std::move(promise)) {
    // nop
  }

  void ref() const noexcept override {
    ref_count_.inc();
  }

  void deref() const noexcept override {
    ref_count_.dec(this);
  }

  void resume(scheduler*, uint64_t) override {
    if (result_.disposed()) {
      return;
    }
    const auto& endpoint = config_->endpoint;
    if (!endpoint.valid() || endpoint.scheme().empty()) {
      result_.set_error(make_error(
        sec::invalid_argument, "HTTP client requires a valid URI endpoint"));
      return;
    }
    // Get host and port for the TCP connection.
    auto auth = endpoint.authority();
    if (auth.host_str().empty()) {
      result_.set_error(
        make_error(sec::invalid_argument, "URI must provide a valid hostname"));
      return;
    }
    if (auth.port == 0) {
      if (endpoint.scheme() == "http")
        auth.port = defaults::net::http_default_port;
      else if (endpoint.scheme() == "https")
        auth.port = defaults::net::https_default_port;
      else {
        result_.set_error(
          make_error(sec::invalid_argument,
                     "unsupported URI scheme: expected http or https"));
        return;
      }
    }
    // Create SSL context if needed.
    std::optional<ssl::context> ctx;
    if (endpoint.scheme() == "https") {
      auto make_ctx = [this] {
        if (config_->context_factory) {
          return (*config_->context_factory)();
        }
        return net::ssl::context::make_client(net::ssl::tls::v1_2);
      };
      if (auto maybe_ctx = make_ctx()) {
        ctx.emplace(std::move(*maybe_ctx));
      } else {
        result_.set_error(std::move(maybe_ctx.error()));
        return;
      }
    }
    // Connect to the server.
    auto try_connect = [this, &auth]() -> expected<stream_socket> {
      if (config_->connector) {
        return config_->connector->connect(auth.host_str(), auth.port,
                                           config_->connection_timeout,
                                           config_->max_retry_count,
                                           config_->retry_delay);
      }
      auto fd = net::make_connected_tcp_stream_socket(
        auth, config_->connection_timeout);
      if (!fd) {
        return caf::unexpected{std::move(fd.error())};
      }
      return stream_socket{*fd};
    };
    auto maybe_fd = try_connect();
    // Connection attempts may take a while: check one more time if the
    // connection is still needed before sending the HTTP request.
    if (result_.disposed()) {
      if (maybe_fd) {
        net::close(*maybe_fd);
      }
      return;
    }
    // Try again later if the connection attempt failed.
    if (!maybe_fd) {
      if (++attempts_ <= config_->max_retry_count) {
        auto self = resumable_ptr{this, add_ref};
        auto when = config_->sys->clock().now() + config_->retry_delay;
        config_->sys->clock().schedule(
          when, make_single_shot_action([self, sys = config_->sys]() mutable {
            auto* impl = detail::actor_system_access{*sys}.impl();
            impl->async_workers().schedule(std::move(self), 0);
          }));
        return;
      }
      result_.set_error(std::move(maybe_fd.error()));
      return;
    }
    // Launch the actual HTTP request; move work to the caf.net multiplexer.
    if (ctx) {
      auto conn = ctx->new_connection(*maybe_fd);
      if (!conn) {
        result_.set_error(std::move(conn.error()));
        return;
      }
      start_request(config_, method_, std::move(payload_), result_,
                    std::move(*conn));
    } else {
      start_request(config_, method_, std::move(payload_), result_, *maybe_fd);
    }
  }

private:
  mutable detail::atomic_ref_count ref_count_;

  const_async_client_config_ptr config_;

  http::method method_;

  byte_buffer payload_;

  caf::async::promise<response> result_;

  size_t attempts_ = 0;
};

async_client_factory::future_t
send_request(const_async_client_config_ptr config, http::method method,
             byte_buffer payload) {
  auto result = caf::async::promise<response>{};
  auto result_future = result.get_future();
  if (config->err.valid()) {
    result.set_error(config->err);
    return result_future;
  }
  auto starter = make_counted<request_starter>(config, method,
                                               std::move(payload),
                                               std::move(result));
  auto* sys = detail::actor_system_access{*config->sys}.impl();
  sys->async_workers().schedule(std::move(starter), 0);
  return result_future;
}

} // namespace

async_client_factory_builder::async_client_factory_builder(actor_system& sys,
                                                           uri endpoint)
  : config_(make_counted<async_client_config>(sys)) {
  config_->endpoint = std::move(endpoint);
}

async_client_factory_builder::async_client_factory_builder(
  actor_system& sys, expected<uri> endpoint)
  : config_(make_counted<async_client_config>(sys)) {
  if (endpoint) {
    config_->endpoint = std::move(*endpoint);
  } else {
    config_->err = std::move(endpoint.error());
  }
}

async_client_factory_builder::~async_client_factory_builder() noexcept {
  // nop
}

async_client_factory_builder&&
async_client_factory_builder::retry_delay(timespan value) && {
  config_->retry_delay = value;
  return std::move(*this);
}

async_client_factory_builder&&
async_client_factory_builder::connection_timeout(timespan value) && {
  config_->connection_timeout = value;
  return std::move(*this);
}

async_client_factory_builder&&
async_client_factory_builder::max_response_size(size_t value) && {
  config_->max_response_size = value;
  return std::move(*this);
}

async_client_factory_builder&&
async_client_factory_builder::max_retry_count(size_t value) && {
  config_->max_retry_count = value;
  return std::move(*this);
}

async_client_factory_builder&&
async_client_factory_builder::add_header_field(std::string name,
                                               std::string value) && {
  do_add_header_field(std::move(name), std::move(value));
  return std::move(*this);
}

async_client_factory_builder&& async_client_factory_builder::connector(
  std::unique_ptr<detail::connector>&& ptr) && {
  config_->connector = std::move(ptr);
  return std::move(*this);
}

async_client_factory async_client_factory_builder::factory() && {
  if (config_->endpoint.valid()) {
    auto host_field = "Host"s;
    if (!config_->fields.contains(host_field)) {
      do_add_header_field(std::move(host_field),
                          host_header_value(config_->endpoint));
    }
  }
  return async_client_factory{std::move(config_)};
}

void async_client_factory_builder::do_add_header_field(std::string name,
                                                       std::string value) {
  config_->fields.emplace(std::move(name), std::move(value));
}

void async_client_factory_builder::set_context_factory(
  unique_callback_ptr<expected<ssl::context>()> fn) {
  config_->context_factory = std::move(fn);
}

async_client_factory::async_client_factory(
  const async_client_factory& other) noexcept
  : config_(other.config_) {
  // nop
}

async_client_factory::async_client_factory(
  async_client_factory&& other) noexcept
  : config_(std::move(other.config_)) {
  // nop
}

async_client_factory&
async_client_factory::operator=(const async_client_factory& other) noexcept {
  config_ = other.config_;
  return *this;
}

async_client_factory&
async_client_factory::operator=(async_client_factory&& other) noexcept {
  config_ = std::move(other.config_);
  return *this;
}

async_client_factory::async_client_factory(
  const_async_client_config_ptr cfg) noexcept
  : config_(std::move(cfg)) {
  // nop
}

async_client_factory::~async_client_factory() noexcept {
  // nop
}

async_client_factory::future_t
async_client_factory::request_impl(method method, byte_buffer payload) const {
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

async_client_factory::future_t async_client_factory::get() const {
  return send_request(config_, http::method::get, {});
}

async_client_factory::future_t async_client_factory::head() const {
  return send_request(config_, http::method::head, {});
}

async_client_factory::future_t
async_client_factory::post_impl(byte_buffer payload) const {
  return send_request(config_, http::method::post, std::move(payload));
}

async_client_factory::future_t
async_client_factory::put_impl(byte_buffer payload) const {
  return send_request(config_, http::method::put, std::move(payload));
}

async_client_factory::future_t async_client_factory::del() const {
  return send_request(config_, http::method::del, {});
}

async_client_factory::future_t async_client_factory::connect() const {
  return send_request(config_, http::method::connect, {});
}

async_client_factory::future_t
async_client_factory::options_impl(byte_buffer payload) const {
  return send_request(config_, http::method::options, std::move(payload));
}

async_client_factory::future_t
async_client_factory::trace_impl(byte_buffer payload) const {
  return send_request(config_, http::method::trace, std::move(payload));
}

} // namespace caf::net::http
