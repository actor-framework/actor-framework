// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/checked_socket.hpp"
#include "caf/net/dsl/client_factory_base.hpp"
#include "caf/net/dsl/generic_config.hpp"
#include "caf/net/http/async_client.hpp"
#include "caf/net/http/client.hpp"
#include "caf/net/http/response.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/ssl/connection.hpp"
#include "caf/net/ssl/transport.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/async/future.hpp"
#include "caf/async/promise.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/detail/forward_like.hpp"
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
  : public dsl::client_factory_base<client_factory> {
public:
  template <class Token, class... Args>
  client_factory(Token token, const dsl::generic_config_value& from,
                 Args&&... args) {
    init_config(from.mpx).assign(from, token, std::forward<Args>(args)...);
  }

  client_factory(client_factory&& other) noexcept;

  client_factory& operator=(client_factory&& other) noexcept;

  ~client_factory() override;

  /// Add an additional HTTP header field to the request.
  client_factory& add_header_field(std::string key, std::string value);

  /// Add an additional HTTP header fields to the request.
  template <class KeyValueMap>
  client_factory& add_header_fields(KeyValueMap&& kv_map) {
    for (auto& [key, value] : kv_map) {
      add_header_field(detail::forward_like<KeyValueMap>(key),
                       detail::forward_like<KeyValueMap>(value));
    }
    return *this;
  }

  /// Sends an HTTP GET message.
  expected<std::pair<async::future<response>, disposable>> get();

  /// Sends an HTTP HEAD message.
  expected<std::pair<async::future<response>, disposable>> head();

  /// Sends an HTTP POST message.
  expected<std::pair<async::future<response>, disposable>>
  post(std::string_view payload);

  /// Sends an HTTP PUT message.
  expected<std::pair<async::future<response>, disposable>>
  put(std::string_view payload);

  /// Sends an HTTP DELETE message.
  expected<std::pair<async::future<response>, disposable>> del();

  /// Sends an HTTP CONNECT message.
  expected<std::pair<async::future<response>, disposable>> connect();

  /// Sends an HTTP OPTIONS message.
  expected<std::pair<async::future<response>, disposable>>
  options(std::string_view payload);

  /// Sends an HTTP TRACE message.
  expected<std::pair<async::future<response>, disposable>>
  trace(std::string_view payload);

  /// Utility function to make a request with given parameters.
  expected<std::pair<async::future<response>, disposable>>
  request(http::method method, std::string_view payload = std::string_view{});

  /// Utility function to make a request with given parameters.
  expected<std::pair<async::future<response>, disposable>>
  request(http::method method, const_byte_span payload);

protected:
  dsl::client_config_value& base_config() override;

private:
  class config_impl;

  using return_t = expected<std::pair<async::future<response>, disposable>>;

  dsl::client_config_value& init_config(multiplexer* mpx);

  template <typename Conn>
  return_t
  do_start_impl(Conn conn, http::method method, const_byte_span payload);

  return_t do_start(dsl::client_config::lazy& data, http::method method,
                    const_byte_span payload);

  return_t do_start(error err);

  config_impl* config_ = nullptr;
};

} // namespace caf::net::http
