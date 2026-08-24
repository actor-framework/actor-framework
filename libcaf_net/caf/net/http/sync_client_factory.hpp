// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/http/method.hpp"
#include "caf/net/http/response.hpp"
#include "caf/net/ssl/context.hpp"

#include "caf/byte_buffer.hpp"
#include "caf/callback.hpp"
#include "caf/detail/concepts.hpp"
#include "caf/detail/connector.hpp"
#include "caf/detail/forward_like.hpp"
#include "caf/fwd.hpp"

#include <string>
#include <type_traits>
#include <utility>

namespace caf::net::http {

/// Factory for launching synchronous HTTP clients. The `sync_client_factory`
/// object is tied to an HTTP endpoint that has been configured via the
/// `sync_client_factory_builder` DSL.
class CAF_NET_EXPORT sync_client_factory {
public:
  friend class sync_client_factory_builder;

  using result_t = expected<response>;

  ~sync_client_factory() noexcept;

  [[nodiscard]] result_t request(method method) const {
    return request_impl(method, {});
  }

  template <detail::char_or_byte_payload T>
  [[nodiscard]] result_t request(method method, T&& payload) const {
    using payload_t = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<payload_t, byte_buffer>)
      return request_impl(method, std::forward<T>(payload));
    else
      return request_impl(method,
                          to_byte_buffer(payload.data(), payload.size()));
  }

  [[nodiscard]] result_t get() const;

  [[nodiscard]] result_t head() const;

  template <detail::char_or_byte_payload T>
  [[nodiscard]] result_t post(T&& payload) const {
    using payload_t = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<payload_t, byte_buffer>) {
      return post_impl(std::forward<T>(payload));
    } else {
      return post_impl(to_byte_buffer(payload.data(), payload.size()));
    }
  }

  template <detail::char_or_byte_payload T>
  [[nodiscard]] result_t put(T&& payload) const {
    using payload_t = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<payload_t, byte_buffer>)
      return put_impl(std::forward<T>(payload));
    else
      return put_impl(to_byte_buffer(payload.data(), payload.size()));
  }

  [[nodiscard]] result_t del() const;

  [[nodiscard]] result_t connect() const;

  template <detail::char_or_byte_payload T>
  [[nodiscard]] result_t options(T&& payload) const {
    using payload_t = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<payload_t, byte_buffer>)
      return options_impl(std::forward<T>(payload));
    else
      return options_impl(to_byte_buffer(payload.data(), payload.size()));
  }

  template <detail::char_or_byte_payload T>
  [[nodiscard]] result_t trace(T&& payload) const {
    using payload_t = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<payload_t, byte_buffer>)
      return trace_impl(std::forward<T>(payload));
    else
      return trace_impl(to_byte_buffer(payload.data(), payload.size()));
  }

private:
  explicit sync_client_factory(const_sync_client_config_ptr cfg) noexcept
    : config_(std::move(cfg)) {
    // nop
  }

  result_t request_impl(method method, byte_buffer payload) const;

  result_t post_impl(byte_buffer payload) const;

  result_t put_impl(byte_buffer payload) const;

  result_t options_impl(byte_buffer payload) const;

  result_t trace_impl(byte_buffer payload) const;

  const_sync_client_config_ptr config_;
};

/// Builder for configuring a new `sync_client_factory`.
class CAF_NET_EXPORT sync_client_factory_builder {
public:
  sync_client_factory_builder() = delete;

  sync_client_factory_builder(const sync_client_factory_builder&) = delete;

  sync_client_factory_builder& operator=(const sync_client_factory_builder&)
    = delete;

  sync_client_factory_builder(sync_client_factory_builder&&) noexcept = default;

  sync_client_factory_builder& operator=(sync_client_factory_builder&&) noexcept
    = default;

  sync_client_factory_builder(actor_system& sys, uri endpoint);

  sync_client_factory_builder(actor_system& sys, expected<uri> endpoint);

  using result_t = expected<response>;

  ~sync_client_factory_builder() noexcept;

  template <class F>
  [[nodiscard]] sync_client_factory_builder&& context(F factory) && {
    static_assert(std::is_same_v<decltype(factory()), expected<ssl::context>>);
    using impl_t = callback_impl<F, expected<ssl::context>()>;
    set_context_factory(std::make_unique<impl_t>(std::move(factory)));
    return std::move(*this);
  }

  [[nodiscard]] sync_client_factory_builder&& retry_delay(timespan value) &&;

  [[nodiscard]] sync_client_factory_builder&&
  connection_timeout(timespan value) &&;

  [[nodiscard]] sync_client_factory_builder&&
  max_response_size(size_t value) &&;

  [[nodiscard]] sync_client_factory_builder&& max_retry_count(size_t value) &&;

  [[nodiscard]] sync_client_factory_builder&&
  add_header_field(std::string name, std::string value) &&;

  template <class KeyValueMap>
  [[nodiscard]] sync_client_factory_builder&&
  add_header_fields(KeyValueMap&& kv_map) && {
    for (auto&& [key, value] : std::forward<KeyValueMap>(kv_map)) {
      do_add_header_field(detail::forward_like<KeyValueMap>(key),
                          detail::forward_like<KeyValueMap>(value));
    }
    return std::move(*this);
  }

  [[nodiscard]] sync_client_factory_builder&&
  connector(std::unique_ptr<detail::connector>&&) &&;

  [[nodiscard]] sync_client_factory factory() &&;

  [[nodiscard]] result_t request(method method) && {
    return std::move(*this).factory().request(method);
  }

  template <detail::char_or_byte_payload T>
  [[nodiscard]] result_t request(method method, T&& payload) && {
    return std::move(*this).factory().request(method, std::forward<T>(payload));
  }

  [[nodiscard]] result_t get() && {
    return std::move(*this).factory().get();
  }

  [[nodiscard]] result_t head() && {
    return std::move(*this).factory().head();
  }

  template <detail::char_or_byte_payload T>
  [[nodiscard]] result_t post(T&& payload) && {
    return std::move(*this).factory().post(std::forward<T>(payload));
  }

  template <detail::char_or_byte_payload T>
  [[nodiscard]] result_t put(T&& payload) && {
    return std::move(*this).factory().put(std::forward<T>(payload));
  }

  [[nodiscard]] result_t del() && {
    return std::move(*this).factory().del();
  }

  [[nodiscard]] result_t connect() && {
    return std::move(*this).factory().connect();
  }

  template <detail::char_or_byte_payload T>
  [[nodiscard]] result_t options(T&& payload) && {
    return std::move(*this).factory().options(std::forward<T>(payload));
  }

  template <detail::char_or_byte_payload T>
  [[nodiscard]] result_t trace(T&& payload) && {
    return std::move(*this).factory().trace(std::forward<T>(payload));
  }

private:
  void do_add_header_field(std::string name, std::string value);

  void set_context_factory(unique_callback_ptr<expected<ssl::context>()> fn);

  sync_client_config_ptr config_;
};

} // namespace caf::net::http
