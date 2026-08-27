// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/lp/size_field_type.hpp"
#include "caf/net/ssl/context.hpp"

#include "caf/async/spsc_buffer.hpp"
#include "caf/callback.hpp"
#include "caf/chunk.hpp"
#include "caf/detail/connector.hpp"
#include "caf/fwd.hpp"
#include "caf/uri.hpp"

#include <memory>
#include <type_traits>
#include <utility>

namespace caf::net::lp {

/// Factory for launching asynchronous length-prefix framing clients. The
/// `async_client_factory` object is tied to an endpoint that has been
/// configured via the `async_client_factory_builder` DSL.
/// @warning the `async_client_factory` object stores a reference to the
///          `actor_system` that was used to create it. Ensure that the
///          `actor_system` outlives this object.
class CAF_NET_EXPORT async_client_factory {
public:
  friend class async_client_factory_builder;

  using pull_t = async::consumer_resource<chunk>;

  using push_t = async::producer_resource<chunk>;

  async_client_factory() = default;

  ~async_client_factory() noexcept;

  /// Starts the connection with freshly created buffers and returns them to the
  /// caller.
  /// @returns the buffers for exchanging messages with the connection.
  [[nodiscard]] std::pair<pull_t, push_t> start() const {
    auto [s2a_pull, s2a_push] = async::make_spsc_buffer_resource<chunk>();
    auto [a2s_pull, a2s_push] = async::make_spsc_buffer_resource<chunk>();
    start_impl(std::move(a2s_pull), std::move(s2a_push));
    return {std::move(s2a_pull), std::move(a2s_push)};
  }

  /// Starts the connection with user-provided buffers.
  void start(pull_t pull, push_t push) const {
    start_impl(std::move(pull), std::move(push));
  }

private:
  explicit async_client_factory(const_async_client_config_ptr cfg) noexcept
    : config_(std::move(cfg)) {
    // nop
  }

  void start_impl(pull_t pull, push_t push) const;

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

  using pull_t = async_client_factory::pull_t;

  using push_t = async_client_factory::push_t;

  ~async_client_factory_builder() noexcept;

  /// Sets the optional SSL context factory used to lazily create the SSL
  /// context when needed by the client.
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

  /// Sets the maximum number of connection retry attempts.
  /// @param value The new maximum retry count.
  /// @returns `std::move(*this)` for further chaining.
  [[nodiscard]] async_client_factory_builder&& max_retry_count(size_t value) &&;

  /// Sets the size field type. The default field size is 4 bytes.
  /// @param value The size field type.
  /// @returns `std::move(*this)` for further chaining.
  [[nodiscard]] async_client_factory_builder&&
  size_field(size_field_type value) &&;

  /// Sets the maximum message size.
  /// @param value The maximum message size.
  /// @returns `std::move(*this)` for further chaining.
  [[nodiscard]] async_client_factory_builder&&
  max_message_size(size_t value) &&;

  /// Installs a custom connector for establishing the TCP connection. This
  /// setter is intended for unit tests, not for production use.
  [[nodiscard]] async_client_factory_builder&&
  connector(std::unique_ptr<detail::connector>&&) &&;

  /// Creates a factory object that can be used to start connections to the
  /// configured endpoint.
  [[nodiscard]] async_client_factory factory() &&;

  /// @copydoc async_client_factory::start
  [[nodiscard]] std::pair<pull_t, push_t> start() && {
    return std::move(*this).factory().start();
  }

  /// @copydoc async_client_factory::start
  void start(pull_t pull, push_t push) && {
    std::move(*this).factory().start(std::move(pull), std::move(push));
  }

private:
  void set_context_factory(unique_callback_ptr<expected<ssl::context>()> fn);

  async_client_config_ptr config_;
};

} // namespace caf::net::lp
