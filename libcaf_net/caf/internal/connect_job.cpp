// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/internal/connect_job.hpp"

#include "caf/net/tcp_stream_socket.hpp"

#include "caf/action.hpp"
#include "caf/actor_clock.hpp"
#include "caf/actor_system.hpp"
#include "caf/detail/actor_system_access.hpp"
#include "caf/format_to_unexpected.hpp"
#include "caf/scheduler.hpp"
#include "caf/sec.hpp"

#include <optional>
#include <utility>

namespace caf::internal {

void connect_job::ref() const noexcept {
  ref_count_.inc();
}

void connect_job::deref() const noexcept {
  ref_count_.dec(this);
}

void connect_job::delete_this() const noexcept {
  delete this;
}

void connect_job::resume(scheduler*, uint64_t) {
  if (canceled())
    return;
  const auto& endpoint = config_->endpoint;
  if (!endpoint.valid() || endpoint.scheme().empty()) {
    fail(make_error(sec::invalid_argument,
                    "client requires a valid URI endpoint"));
    return;
  }
  auto auth = endpoint.authority();
  if (auth.host_str().empty()) {
    fail(
      make_error(sec::invalid_argument, "URI must provide a valid hostname"));
    return;
  }
  auto [plain, ssl] = config_->schemes();
  auto use_ssl = false;
  if (endpoint.scheme() == ssl.scheme) {
    use_ssl = true;
  } else if (endpoint.scheme() != plain.scheme) {
    fail(format_to_unexpected(sec::invalid_argument,
                              "unsupported URI scheme: expected {} or {}",
                              plain.scheme, ssl.scheme)
           .error());
    return;
  }
  if (auth.port == 0) {
    auth.port = use_ssl ? ssl.port : plain.port;
  }
  std::optional<net::ssl::context> ctx;
  if (use_ssl) {
    auto make_ctx = [this] {
      if (config_->context_factory)
        return (*config_->context_factory)();
      return net::ssl::context::make_client(net::ssl::tls::v1_2);
    };
    if (auto maybe_ctx = make_ctx()) {
      ctx.emplace(std::move(*maybe_ctx));
    } else {
      fail(std::move(maybe_ctx.error()));
      return;
    }
  }
  auto try_connect = [this, &auth]() -> expected<net::stream_socket> {
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
    return net::stream_socket{*fd};
  };
  auto maybe_fd = try_connect();
  if (canceled()) {
    if (maybe_fd)
      net::close(*maybe_fd);
    return;
  }
  if (!maybe_fd) {
    if (++attempts_ <= config_->max_retry_count) {
      auto self = resumable_ptr{static_cast<resumable*>(this), add_ref};
      auto when = config_->sys->clock().now() + config_->retry_delay;
      config_->sys->clock().schedule(
        when, make_single_shot_action([self, sys = config_->sys]() mutable {
          auto* impl = detail::actor_system_access{*sys}.impl();
          impl->async_workers().schedule(std::move(self), 0);
        }));
      return;
    }
    fail(std::move(maybe_fd.error()));
    return;
  }
  if (ctx) {
    auto conn = ctx->new_connection(*maybe_fd);
    if (!conn) {
      net::close(*maybe_fd);
      fail(std::move(conn.error()));
      return;
    }
    if (ctx->hostname_validation()
        && !conn->hostname(auth.host_str().c_str())) {
      // new_connection() attaches the socket with BIO_NOCLOSE, so it must be
      // closed explicitly here.
      net::close(*maybe_fd);
      fail(format_to_unexpected(sec::protocol_error,
                                "unable to set {} for SSL hostname "
                                "validation",
                                auth.host_str())
             .error());
      return;
    }
    handover(std::move(*conn));
  } else {
    handover(*maybe_fd);
  }
}

} // namespace caf::internal
