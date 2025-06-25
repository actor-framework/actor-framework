// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/acceptor_resource.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/ssl/context.hpp"

#include "caf/actor_cast.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/callback.hpp"
#include "caf/fwd.hpp"

#include <cstdint>

namespace caf::net::octet_stream {

/// Entry point for the `with(...)` DSL.
with_t CAF_NET_EXPORT with(multiplexer* mpx);

/// Entry point for the `with(...)` DSL.
with_t CAF_NET_EXPORT with(actor_system& sys);

class CAF_NET_EXPORT with_t {
public:
  // -- nested types -----------------------------------------------------------

  class config_impl;

  using config_ptr = std::unique_ptr<config_impl>;

  class CAF_NET_EXPORT server {
  public:
    friend class with_t;

    ~server();

    /// Sets the maximum number of connections the server permits.
    [[nodiscard]] server&& max_connections(size_t value) &&;

    /// Monitors the actor handle @p hdl and stops the server if the monitored
    /// actor terminates.
    template <class ActorHandle>
    [[nodiscard]] server&& monitor(const ActorHandle& hdl) && {
      do_monitor(actor_cast<strong_actor_ptr>(hdl));
      return std::move(*this);
    }

    /// Overrides the default buffer size for reading from the network.
    [[nodiscard]] server&& read_buffer_size(uint32_t new_value) &&;

    /// Overrides the default buffer size for writing to the network.
    [[nodiscard]] server&& write_buffer_size(uint32_t new_value) &&;

    /// Starts a connection using an octet stream.
    /// @param on_start The callback to invoke after the connection has started.
    /// @returns On success, a handle to stop the connection. On failure, an
    ///          error.
    /// @note The `on_start` callback is only invoked if the connection started
    ///       successfully.
    template <class OnStart>
    expected<disposable> start(OnStart on_start) {
      static_assert(std::is_invocable_v<OnStart, acceptor_resource<std::byte>>);
      using async::make_spsc_buffer_resource;
      auto [pull, push] = make_spsc_buffer_resource<accept_event<std::byte>>();
      auto res = do_start(std::move(push));
      if (res) {
        on_start(std::move(pull));
      }
      return res;
    }

  private:
    explicit server(config_ptr&& cfg) noexcept;

    using push_t = async::producer_resource<accept_event<std::byte>>;

    void do_monitor(strong_actor_ptr ptr);

    expected<disposable> do_start(push_t push);

    config_ptr config_;
  };

  class CAF_NET_EXPORT client {
  public:
    friend class with_t;

    ~client();

    /// Sets the retry delay for connection attempts.
    /// @param value The new retry delay.
    /// @returns a reference to this `client_factory`.
    [[nodiscard]] client&& retry_delay(timespan value) &&;

    /// Sets the connection timeout for connection attempts.
    /// @param value The new connection timeout.
    /// @returns a reference to this `client_factory`.
    [[nodiscard]] client&& connection_timeout(timespan value) &&;

    /// Sets the maximum number of connection retry attempts.
    /// @param value The new maximum retry count.
    /// @returns a reference to this `client_factory`.
    [[nodiscard]] client&& max_retry_count(size_t value) &&;

    /// Overrides the default buffer size for reading from the network.
    [[nodiscard]] client&& read_buffer_size(uint32_t new_value) &&;

    /// Overrides the default buffer size for writing to the network.
    [[nodiscard]] client&& write_buffer_size(uint32_t new_value) &&;

    template <class OnStart>
    [[nodiscard]] expected<disposable> start(OnStart on_start) {
      static_assert(std::is_invocable_v<OnStart, pull_t, push_t>);
      // Create socket-to-application and application-to-socket buffers.
      auto [s2a_pull, s2a_push] = async::make_spsc_buffer_resource<std::byte>();
      auto [a2s_pull, a2s_push] = async::make_spsc_buffer_resource<std::byte>();
      // Wrap the trait and the buffers that belong to the socket.
      auto res = do_start(std::move(a2s_pull), std::move(s2a_push));
      if (res) {
        on_start(std::move(s2a_pull), std::move(a2s_push));
      }
      return res;
    }

  private:
    using pull_t = async::consumer_resource<std::byte>;

    using push_t = async::producer_resource<std::byte>;

    explicit client(config_ptr&& cfg) noexcept;

    expected<disposable> do_start(pull_t pull, push_t push);

    config_ptr config_;
  };

  explicit with_t(multiplexer* mpx);

  with_t(with_t&&) noexcept = default;

  with_t& operator=(with_t&&) noexcept = default;

  ~with_t();

  /// Sets the optional SSL context.
  /// @param ctx The SSL context for encryption.
  /// @returns a reference to `*this`.
  [[nodiscard]] with_t&& context(ssl::context ctx) &&;

  /// Sets the optional SSL context.
  /// @param ctx The SSL context for encryption. Passing an `expected` with a
  ///            default-constructed `error` results in a no-op.
  /// @returns a reference to `*this`.
  [[nodiscard]] with_t&& context(expected<ssl::context> ctx) &&;

  /// Sets an error handler.
  template <class OnError>
  [[nodiscard]] with_t&& on_error(OnError fn) && {
    static_assert(std::is_invocable_v<OnError, const error&>);
    using impl_t = callback_impl<OnError, void(const error&)>;
    set_on_error(std::make_unique<impl_t>(std::move(fn)));
    return std::move(*this);
  }

  /// Creates a new server factory object for the given TCP `port` and
  /// `bind_address`.
  /// @param port Port number to bind to.
  /// @param bind_address IP address to bind to. Default is an empty string.
  /// @param reuse_addr Whether to set the SO_REUSEADDR option on the socket.
  /// @returns an `server` object initialized with the given parameters.
  [[nodiscard]] server accept(uint16_t port,
                              std::string bind_address = std::string{},
                              bool reuse_addr = true) &&;

  /// Creates a new server factory object for the given accept socket.
  /// @param fd File descriptor for the accept socket.
  /// @returns an `server` object that will start a server on `fd`.
  [[nodiscard]] server accept(tcp_accept_socket fd) &&;

  /// Creates a new server factory object for the given acceptor.
  /// @param acc The SSL acceptor for incoming TCP connections.
  /// @returns an `server` object that will start a server on `acc`.
  [[nodiscard]] server accept(ssl::tcp_acceptor acc) &&;

  /// Creates a new client factory object for the given TCP `host` and `port`.
  /// @param host The hostname or IP address to connect to.
  /// @param port The port number to connect to.
  /// @returns a `client` object initialized with the given parameters.
  [[nodiscard]] client connect(std::string host, uint16_t port) &&;

  /// Creates a new client factory object for the given stream `fd`.
  /// @param fd The stream socket to use for the connection.
  /// @returns a `client` object that will use the given socket.
  [[nodiscard]] client connect(stream_socket fd) &&;

  /// Creates a new client factory object for the given SSL `connection`.
  /// @param conn The SSL connection to use.
  /// @returns a `client` object that will use the given connection.
  [[nodiscard]] client connect(ssl::connection conn) &&;

private:
  using on_error_callback = unique_callback_ptr<void(const error&)>;

  void set_on_error(on_error_callback ptr);

  config_ptr config_;
};

} // namespace caf::net::octet_stream
