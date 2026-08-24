// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/http/method.hpp"
#include "caf/net/http/response.hpp"
#include "caf/net/ssl/context.hpp"

#include "caf/async/future.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/callback.hpp"
#include "caf/detail/concepts.hpp"
#include "caf/detail/connector.hpp"
#include "caf/detail/forward_like.hpp"
#include "caf/fwd.hpp"
#include "caf/uri.hpp"

#include <string>
#include <type_traits>
#include <utility>

namespace caf::net::http {

/// Factory for launching asynchronous HTTP clients. The `async_client_factory`
/// object is tied to an HTTP endpoint that has been configured via the
/// `async_client_factory_builder` DSL.
/// @warning the `async_client_factory` object stores a reference to the
///          `actor_system` that was used to create it. Ensure that the
///          `actor_system` outlives this object.
class CAF_NET_EXPORT async_client_factory {
public:
  friend class async_client_factory_builder;

  using future_t = caf::async::future<response>;

  async_client_factory() = default;

  ~async_client_factory() noexcept;

  /// Sends an HTTP request without a payload.
  [[nodiscard]] future_t request(method method) const {
    return request_impl(method, {});
  }

  /// Sends an HTTP request with a payload.
  template <detail::char_or_byte_payload T>
  [[nodiscard]] future_t request(method method, T&& payload) const {
    using payload_t = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<payload_t, byte_buffer>)
      return request_impl(method, std::forward<T>(payload));
    else
      return request_impl(method,
                          to_byte_buffer(payload.data(), payload.size()));
  }

  /// Sends an HTTP GET message.
  [[nodiscard]] future_t get() const;

  /// Sends an HTTP HEAD message.
  [[nodiscard]] future_t head() const;

  /// Sends an HTTP POST message.
  template <detail::char_or_byte_payload T>
  [[nodiscard]] future_t post(T&& payload) const {
    using payload_t = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<payload_t, byte_buffer>) {
      return post_impl(std::forward<T>(payload));
    } else {
      return post_impl(to_byte_buffer(payload.data(), payload.size()));
    }
  }

  /// Sends an HTTP PUT message.
  template <detail::char_or_byte_payload T>
  [[nodiscard]] future_t put(T&& payload) const {
    using payload_t = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<payload_t, byte_buffer>)
      return put_impl(std::forward<T>(payload));
    else
      return put_impl(to_byte_buffer(payload.data(), payload.size()));
  }

  /// Sends an HTTP DELETE message.
  [[nodiscard]] future_t del() const;

  /// Sends an HTTP CONNECT message.
  [[nodiscard]] future_t connect() const;

  /// Sends an HTTP OPTIONS message.
  template <detail::char_or_byte_payload T>
  [[nodiscard]] future_t options(T&& payload) const {
    using payload_t = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<payload_t, byte_buffer>)
      return options_impl(std::forward<T>(payload));
    else
      return options_impl(to_byte_buffer(payload.data(), payload.size()));
  }

  /// Sends an HTTP TRACE message.
  template <detail::char_or_byte_payload T>
  [[nodiscard]] future_t trace(T&& payload) const {
    using payload_t = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<payload_t, byte_buffer>)
      return trace_impl(std::forward<T>(payload));
    else
      return trace_impl(to_byte_buffer(payload.data(), payload.size()));
  }

private:
  explicit async_client_factory(const_async_client_config_ptr cfg) noexcept
    : config_(std::move(cfg)) {
    // nop
  }

  future_t request_impl(method method, byte_buffer payload) const;

  future_t post_impl(byte_buffer payload) const;

  future_t put_impl(byte_buffer payload) const;

  future_t options_impl(byte_buffer payload) const;

  future_t trace_impl(byte_buffer payload) const;

  const_async_client_config_ptr config_;
};

/// Builder for configuring a new `async_client_factory`.
class CAF_NET_EXPORT async_client_factory_builder {
public:
  async_client_factory_builder() = delete;

  async_client_factory_builder(const async_client_factory_builder&) = delete;

  async_client_factory_builder& operator=(const async_client_factory_builder&)
    = delete;

  async_client_factory_builder(async_client_factory_builder&&) noexcept
    = default;

  async_client_factory_builder&
  operator=(async_client_factory_builder&&) noexcept = default;

  async_client_factory_builder(actor_system& sys, uri endpoint);

  async_client_factory_builder(actor_system& sys, expected<uri> endpoint);

  using future_t = caf::async::future<response>;

  ~async_client_factory_builder() noexcept;

  /// Sets the optional SSL context factory used to lazily create the SSL
  /// context when needed by the client. Isn't used when creating servers.
  /// @param factory The factory that creates the SSL context for encryption.
  /// @returns `std::move(*this)` for further chaining.
  template <class F>
  [[nodiscard]] async_client_factory_builder&& context(F factory) && {
    static_assert(std::is_same_v<decltype(factory()), expected<ssl::context>>);
    using impl_t = callback_impl<F, expected<ssl::context>()>;
    set_context_factory(std::make_unique<impl_t>(std::move(factory)));
    return std::move(*this);
  }

  /// Sets the retry delay for connection attempts.
  /// @param value The new retry delay.
  /// @returns `std::move(*this)` for further chaining.
  [[nodiscard]] async_client_factory_builder&& retry_delay(timespan value) &&;

  /// Sets the connection timeout for connection attempts.
  /// @param value The new connection timeout.
  /// @returns `std::move(*this)` for further chaining.
  [[nodiscard]] async_client_factory_builder&&
  connection_timeout(timespan value) &&;

  /// Sets the maximum response size to @p value. Defaults to 512KiB.
  [[nodiscard]] async_client_factory_builder&&
  max_response_size(size_t value) &&;

  /// Sets the maximum number of connection retry attempts.
  /// @param value The new maximum retry count.
  /// @returns `std::move(*this)` for further chaining.
  [[nodiscard]] async_client_factory_builder&& max_retry_count(size_t value) &&;

  /// Adds an additional HTTP header field to the request.
  /// @param name The name of the new field.
  /// @param value The value of the new field.
  /// @returns `std::move(*this)` for further chaining.
  [[nodiscard]] async_client_factory_builder&&
  add_header_field(std::string name, std::string value) &&;

  /// Adds additional HTTP header fields to the request.
  /// @param kv_map A container of key-value pairs to insert as fields.
  /// @returns `std::move(*this)` for further chaining.
  template <class KeyValueMap>
  [[nodiscard]] async_client_factory_builder&&
  add_header_fields(KeyValueMap&& kv_map) && {
    for (auto&& [key, value] : std::forward<KeyValueMap>(kv_map)) {
      do_add_header_field(detail::forward_like<KeyValueMap>(key),
                          detail::forward_like<KeyValueMap>(value));
    }
    return std::move(*this);
  }

  /// Installs a custom connector for establishing the TCP connection. This
  /// setter is intended for unit tests, not for production use.
  [[nodiscard]] async_client_factory_builder&&
  connector(std::unique_ptr<detail::connector>&&) &&;

  /// Creates a factory object that can be used to send HTTP requests to the
  /// configured endpoint.
  [[nodiscard]] async_client_factory factory() &&;

  /// Sends an HTTP request without a payload.
  [[nodiscard]] future_t request(method method) && {
    return std::move(*this).factory().request(method);
  }

  /// Sends an HTTP request with a payload.
  template <detail::char_or_byte_payload T>
  [[nodiscard]] future_t request(method method, T&& payload) && {
    return std::move(*this).factory().request(method, std::forward<T>(payload));
  }

  /// @copydoc async_client_factory::get
  [[nodiscard]] future_t get() && {
    return std::move(*this).factory().get();
  }

  /// @copydoc async_client_factory::head
  [[nodiscard]] future_t head() && {
    return std::move(*this).factory().head();
  }

  /// @copydoc async_client_factory::post
  template <detail::char_or_byte_payload T>
  [[nodiscard]] future_t post(T&& payload) && {
    return std::move(*this).factory().post(std::forward<T>(payload));
  }

  /// @copydoc async_client_factory::put
  template <detail::char_or_byte_payload T>
  [[nodiscard]] future_t put(T&& payload) && {
    return std::move(*this).factory().put(std::forward<T>(payload));
  }

  /// @copydoc async_client_factory::del
  [[nodiscard]] future_t del() && {
    return std::move(*this).factory().del();
  }

  /// @copydoc async_client_factory::connect
  [[nodiscard]] future_t connect() && {
    return std::move(*this).factory().connect();
  }

  /// @copydoc async_client_factory::options
  template <detail::char_or_byte_payload T>
  [[nodiscard]] future_t options(T&& payload) && {
    return std::move(*this).factory().options(std::forward<T>(payload));
  }

  /// @copydoc async_client_factory::trace
  template <detail::char_or_byte_payload T>
  [[nodiscard]] future_t trace(T&& payload) && {
    return std::move(*this).factory().trace(std::forward<T>(payload));
  }

private:
  void do_add_header_field(std::string name, std::string value);

  void set_context_factory(unique_callback_ptr<expected<ssl::context>()> fn);

  async_client_config_ptr config_;
};

} // namespace caf::net::http
