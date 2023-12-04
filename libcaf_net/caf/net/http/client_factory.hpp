// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/checked_socket.hpp"
#include "caf/net/dsl/client_factory_base.hpp"
#include "caf/net/http/async_client.hpp"
#include "caf/net/http/client.hpp"
#include "caf/net/http/config.hpp"
#include "caf/net/http/response.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/ssl/connection.hpp"
#include "caf/net/ssl/transport.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/async/future.hpp"
#include "caf/async/promise.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/detail/latch.hpp"
#include "caf/disposable.hpp"
#include "caf/timespan.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>
#include <variant>

namespace caf::net::http {

/// Factory for the `with(...).connect(...).request(...)` DSL.
class CAF_NET_EXPORT client_factory
  : public dsl::client_factory_base<client_config, client_factory> {
public:
  using super = dsl::client_factory_base<client_config, client_factory>;

  using config_type = client_config;

  using super::super;

  /// Add an additional HTTP header field to the request.
  client_factory& add_header_field(std::string key, std::string value) {
    config().fields.insert(std::pair{std::move(key), std::move(value)});
    return *this;
  }

  /// Sends an HTTP GET message.
  expected<std::pair<async::future<response>, disposable>> get() {
    return request(http::method::get);
  }

  /// Sends an HTTP HEAD message.
  expected<std::pair<async::future<response>, disposable>> head() {
    return request(http::method::head);
  }

  /// Sends an HTTP POST message.
  expected<std::pair<async::future<response>, disposable>>
  post(std::string_view payload) {
    return request(http::method::post, payload);
  }

  /// Sends an HTTP PUT message.
  expected<std::pair<async::future<response>, disposable>>
  put(std::string_view payload) {
    return request(http::method::put, payload);
  }

  /// Sends an HTTP DELETE message.
  expected<std::pair<async::future<response>, disposable>> del() {
    return request(http::method::del);
  }

  /// Sends an HTTP CONNECT message.
  expected<std::pair<async::future<response>, disposable>> connect() {
    return request(http::method::connect);
  }

  /// Sends an HTTP OPTIONS message.
  expected<std::pair<async::future<response>, disposable>>
  options(std::string_view payload) {
    return request(http::method::options, payload);
  }

  /// Sends an HTTP TRACE message.
  expected<std::pair<async::future<response>, disposable>>
  trace(std::string_view payload) {
    return request(http::method::trace, payload);
  }

  /// Utility function to make a request with given parameters.
  expected<std::pair<async::future<response>, disposable>>
  request(http::method method, std::string_view payload = std::string_view{});

  /// Utility function to make a request with given parameters.
  expected<std::pair<async::future<response>, disposable>>
  request(http::method method, const_byte_span payload);

private:
  using return_t = expected<std::pair<async::future<response>, disposable>>;

  template <typename Conn>
  return_t do_start_impl(config_type& cfg, Conn conn, http::method method,
                         const_byte_span payload);

  return_t do_start(config_type& cfg, dsl::client_config::lazy& data,
                    http::method method, const_byte_span payload);

  return_t do_start(config_type& cfg, error err) {
    cfg.call_on_error(err);
    return return_t{std::move(err)};
  }
};

} // namespace caf::net::http
