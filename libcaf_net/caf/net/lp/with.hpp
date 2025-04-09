// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/accept_event.hpp"
#include "caf/net/lp/frame.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/tcp_accept_socket.hpp"

#include "caf/actor_cast.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/callback.hpp"
#include "caf/disposable.hpp"
#include "caf/fwd.hpp"

#include <cstdint>

namespace caf::net::lp {

/// Entry point for the `with(...)` DSL.
with_t CAF_NET_EXPORT with(multiplexer* mpx);

/// Entry point for the `with(...)` DSL.
with_t CAF_NET_EXPORT with(actor_system& sys);

/// Factory for creating HTTP servers and clients.
class CAF_NET_EXPORT with_t {
public:
  // -- nested types -----------------------------------------------------------
  class config_impl;

  using config_ptr = std::unique_ptr<config_impl>;

  /// Factory for creating HTTP servers.
  class CAF_NET_EXPORT server {
  public:
    friend class with_t;

    ~server();

    /// Sets the maximum number of connections the server permits.
    [[nodiscard]] server&& max_connections(size_t value) &&;

    /// Configures whether the server creates its socket with `SO_REUSEADDR`.
    [[nodiscard]] server&& reuse_address(bool value) &&;

    /// Monitors the actor handle @p hdl and stops the server if the monitored
    /// actor terminates.
    template <class ActorHandle>
    [[nodiscard]] server&& monitor(const ActorHandle& hdl) && {
      do_monitor(actor_cast<strong_actor_ptr>(hdl));
      return std::move(*this);
    }

    /// Starts a server that accepts incoming connections and invokes `handler`
    /// with the pull resource.
    /// @param handler The callback to invoke after the server has started.
    /// @returns On success, a handle to stop the server. On failure, an error.
    /// @note The `handler` is only invoked if the server started successfully.
    template <class OnStart>
    [[nodiscard]] expected<disposable> start(OnStart on_start) && {
      static_assert(std::is_invocable_v<OnStart, pull_t>);
      auto [pull, push]
        = async::make_spsc_buffer_resource<accept_event<frame>>();
      auto res = do_start(std::move(push));
      if (res) {
        on_start(std::move(pull));
      }
      return res;
    }

  private:
    explicit server(config_ptr&& cfg) noexcept;

    using push_t = async::producer_resource<accept_event<frame>>;

    using pull_t = async::consumer_resource<accept_event<frame>>;

    void do_monitor(strong_actor_ptr ptr);

    expected<disposable> do_start(push_t push);

    config_ptr config_;
  };

  /// Factory for creating HTTP clients.
  class CAF_NET_EXPORT client {
  public:
    friend class with_t;

    ~client();

    /// Sets the retry delay for connection attempts.
    /// @param value The new retry delay.
    /// @returns a reference to this `client`.
    [[nodiscard]] client&& retry_delay(timespan value) &&;

    /// Sets the connection timeout for connection attempts.
    /// @param value The new connection timeout.
    /// @returns a reference to this `client`.
    [[nodiscard]] client&& connection_timeout(timespan value) &&;

    /// Sets the maximum number of connection retry attempts.
    /// @param value The new maximum retry count.
    /// @returns a reference to this `client`.
    [[nodiscard]] client&& max_retry_count(size_t value) &&;

    /// Starts a new connection with the length prefix protocol.
    /// @param on_start The callback to invoke after the connection has started.
    /// @returns On success, a handle to stop the connection. On failure, an
    ///          error.
    /// @note The `on_start` callback is only invoked if the connection started
    ///       successfully.
    template <class OnStart>
    [[nodiscard]] expected<disposable> start(OnStart on_start) {
      // Create socket-to-application and application-to-socket buffers.
      auto [s2a_pull, s2a_push] = async::make_spsc_buffer_resource<frame>();
      auto [a2s_pull, a2s_push] = async::make_spsc_buffer_resource<frame>();
      // Wrap the trait and the buffers that belong to the socket.
      auto res = do_start(std::move(a2s_pull), std::move(s2a_push));
      if (res) {
        on_start(std::move(s2a_pull), std::move(a2s_push));
      }
      return res;
    }

  private:
    explicit client(config_ptr&& cfg) noexcept;

    using pull_t = async::consumer_resource<frame>;

    using push_t = async::producer_resource<frame>;

    expected<disposable> do_start(pull_t, push_t);

    config_ptr config_;
  };

  // -- constructors, destructors, and assignment operators --------------------

  explicit with_t(multiplexer* mpx);

  with_t(with_t&&) noexcept;

  with_t& operator=(with_t&&) noexcept;

  ~with_t();

  // -- methods for building the HTTP server/client ----------------------------

  /// Sets the optional SSL context.
  /// @param ctx The SSL context for encryption.
  /// @returns a reference to `*this`.
  [[nodiscard]] with_t&& context(ssl::context ctx) &&;

  /// Sets the optional SSL context.
  /// @param ctx The SSL context for encryption. Passing an `expected` with a
  ///            default-constructed `error` results in a no-op.
  /// @returns a reference to `*this`.
  [[nodiscard]] with_t&& context(expected<ssl::context> ctx) &&;

  /// Sets the optional SSL context factory used to lazily create the SSL
  /// context when needed by the client. Isn't used when creating servers.
  /// @param factory The factory that creates the SSL context  for encryption.
  /// @returns a reference to `*this`.
  template <typename F>
  [[nodiscard]] with_t&& context_factory(F factory) && {
    static_assert(std::is_same_v<decltype(factory()), expected<ssl::context>>);
    using impl_t = callback_impl<F, expected<ssl::context>()>;
    set_context_factory(std::make_unique<impl_t>(std::move(factory)));
    return std::move(*this);
  }

  /// Sets an error handler.
  template <class OnError>
  [[nodiscard]] with_t&& on_error(OnError fn) && {
    static_assert(std::is_invocable_v<OnError, const error&>);
    using impl_t = callback_impl<OnError, void(const error&)>;
    set_on_error(std::make_unique<impl_t>(std::move(fn)));
    return std::move(*this);
  }

  /// Creates a `server` object for the given TCP `port` and `bind_address`.
  /// @param port Port number to bind to.
  /// @param bind_address IP address to bind to. Default is an empty string.
  /// @param reuse_addr Whether to set the SO_REUSEADDR option on the socket.
  /// @returns an `server` object initialized with the given parameters.
  [[nodiscard]] server accept(uint16_t port,
                              std::string bind_address = std::string{},
                              bool reuse_addr = true) &&;

  /// Creates an `server` object for the given accept socket.
  /// @param fd File descriptor for the accept socket.
  /// @returns an `server` object that will start a server on `fd`.
  [[nodiscard]] server accept(tcp_accept_socket fd) &&;

  /// Creates an `server` object for the given acceptor.
  /// @param acc The SSL acceptor for incoming TCP connections.
  /// @returns an `server` object that will start a server on `acc`.
  [[nodiscard]] server accept(ssl::tcp_acceptor acc) &&;

  /// Creates a `client` object for the given TCP `host` and `port`.
  /// @param host The hostname or IP address to connect to.
  /// @param port The port number to connect to.
  /// @returns a `client` object initialized with the given parameters.
  [[nodiscard]] client connect(std::string host, uint16_t port) &&;

  /// Creates a `client` object for the given stream `fd`.
  /// @param fd The stream socket to use for the connection.
  /// @returns a `client` object that will use the given socket.
  [[nodiscard]] client connect(stream_socket fd) &&;

  /// Creates a `client` object for the given SSL `connection`.
  /// @param conn The SSL connection to use.
  /// @returns a `client` object that will use the given connection.
  [[nodiscard]] client connect(ssl::connection conn) &&;

private:
  using on_error_callback = unique_callback_ptr<void(const error&)>;

  void set_on_error(on_error_callback ptr);

  void set_context_factory(unique_callback_ptr<expected<ssl::context>()> fn);

  config_ptr config_;
};

} // namespace caf::net::lp
