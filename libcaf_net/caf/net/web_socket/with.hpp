// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"

#include "caf/actor_cast.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/callback.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/detail/ws_conn_acceptor.hpp"
#include "caf/expected.hpp"
#include "caf/fwd.hpp"

#include <cstdint>
#include <memory>
#include <type_traits>

namespace caf::net::web_socket {

/// Entry point for the `with(...)` DSL.
with_t CAF_NET_EXPORT with(multiplexer* mpx);

/// Entry point for the `with(...)` DSL.
with_t CAF_NET_EXPORT with(actor_system& sys);

/// Factory for creating WebSocket servers and clients.
class CAF_NET_EXPORT with_t {
public:
  // -- friends ----------------------------------------------------------------

  template <class...>
  friend class server_launcher;

  // -- nested types -----------------------------------------------------------

  class config_impl;

  class server;

  class client;

  using config_ptr = std::unique_ptr<config_impl>;

  /// Implementation detail for `server_launcher`.
  class CAF_NET_EXPORT server_launcher_base {
  public:
    virtual ~server_launcher_base();

  protected:
    server_launcher_base(config_ptr&& cfg) noexcept;

    expected<disposable> do_start();

    config_ptr config_;
  };

  /// Final step of a server configuration after defining the `on_request`
  /// handler.
  template <class... Ts>
  class server_launcher : public server_launcher_base {
  public:
    friend class server;

    using super = server_launcher_base;

    using accept_event = cow_tuple<async::consumer_resource<frame>,
                                   async::producer_resource<frame>, Ts...>;

    using pull_t = async::consumer_resource<accept_event>;

    /// Starts the server and invokes `handler` with the pull resource.
    /// @param handler The callback to invoke after the server has started.
    /// @returns On success, a handle to stop the server. On failure, an error.
    /// @note The `handler` is only invoked if the server started successfully.
    template <class Handler>
    [[nodiscard]] expected<disposable> start(Handler&& handler) && {
      static_assert(std::is_invocable_v<Handler, pull_t>);
      auto res = this->do_start();
      if (res) {
        std::forward<Handler>(handler)(std::move(pull_));
      }
      return res;
    }

  private:
    server_launcher(config_ptr&& cfg, pull_t&& pull)
      : super(std::move(cfg)), pull_(std::move(pull)) {
      // nop
    }

    pull_t pull_;
  };

  /// Factory for creating WebSocket servers.
  class CAF_NET_EXPORT server {
  public:
    friend class with_t;

    ~server();

    /// Sets the handler for incoming connection requests.
    /// @param handler The callback to invoke for incoming requests.
    /// @returns The next factory object for starting the server.
    template <class Handler>
    [[nodiscard]] auto on_request(Handler&& handler) {
      // Type checking.
      using fn_t = std::decay_t<Handler>;
      using fn_trait = detail::get_callable_trait_t<fn_t>;
      static_assert(fn_trait::num_args == 1,
                    "on_request must take exactly one argument");
      using arg_types = typename fn_trait::arg_types;
      using arg1_t = detail::tl_at_t<arg_types, 0>;
      using acceptor_t = std::decay_t<arg1_t>;
      static_assert(is_acceptor_v<acceptor_t>,
                    "on_request must take an acceptor as its argument");
      static_assert(std::is_same_v<arg1_t, acceptor_t&>,
                    "on_request must take the acceptor as mutable reference");
      // Wrap the callback and return the next factory object.
      using impl_t = detail::ws_conn_acceptor_t<fn_t, acceptor_t>;
      using accept_event_t = typename impl_t::accept_event;
      auto [pull, push] = async::make_spsc_buffer_resource<accept_event_t>();
      auto wsa = make_counted<impl_t>(std::forward<Handler>(handler),
                                      std::move(push));
      set_acceptor(std::move(wsa));
      return server_launcher{std::move(config_), std::move(pull)};
    }

    /// Sets the maximum number of connections the server permits.
    [[nodiscard]] server&& max_connections(size_t value) &&;

    /// Monitors the actor handle @p hdl and stops the server if the monitored
    /// actor terminates.
    template <class ActorHandle>
    [[nodiscard]] server&& monitor(const ActorHandle& hdl) && {
      do_monitor(actor_cast<strong_actor_ptr>(hdl));
      return std::move(*this);
    }

  private:
    explicit server(config_ptr&& cfg) noexcept;

    void do_monitor(strong_actor_ptr ptr);

    void set_acceptor(detail::ws_conn_acceptor_ptr acc);

    config_ptr config_;
  };

  class CAF_NET_EXPORT client {
  public:
    friend class with_t;

    using pull_t = async::consumer_resource<frame>;

    using push_t = async::producer_resource<frame>;

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

    /// @copydoc handshake::protocols
    [[nodiscard]] client&& protocols(std::string value) &&;

    /// @copydoc handshake::extensions
    [[nodiscard]] client&& extensions(std::string value) &&;

    /// @copydoc handshake::field
    [[nodiscard]] client&& header_field(std::string_view key,
                                        std::string value) &&;

    /// Starts a connection with the WebSocket protocol.
    /// @param on_start The callback to invoke after the connection has started.
    /// @returns On success, a handle to stop the connection. On failure, an
    ///          error.
    /// @note The `on_start` callback is only invoked if the connection started
    ///       successfully.
    template <class OnStart>
    [[nodiscard]] expected<disposable> start(OnStart on_start) && {
      static_assert(std::is_invocable_v<OnStart, pull_t, push_t>);
      // Create socket-to-application and application-to-socket buffers.
      auto [s2a_pull, s2a_push] = async::make_spsc_buffer_resource<frame>();
      auto [a2s_pull, a2s_push] = async::make_spsc_buffer_resource<frame>();
      // Wrap the trait and the buffers that belong to the socket.
      auto res = do_start(a2s_pull, s2a_push);
      if (res) {
        on_start(std::move(s2a_pull), std::move(a2s_push));
      }
      return res;
    }

  private:
    explicit client(config_ptr&& cfg) noexcept;

    expected<disposable> do_start(pull_t&, push_t&);

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

  /// Creates a new client factory object for the given TCP `endpoint`.
  /// @param endpoint The endpoint of the TCP server to connect to.
  /// @returns a `client` object initialized with the given parameters.
  [[nodiscard]] client connect(uri endpoint) &&;

  /// Creates a new client factory object for the given TCP `endpoint`.
  /// @param endpoint The endpoint of the TCP server to connect to.
  /// @returns a `client` object initialized with the given parameters.
  [[nodiscard]] client connect(expected<uri> endpoint) &&;

private:
  using on_error_callback = unique_callback_ptr<void(const error&)>;

  void set_on_error(on_error_callback ptr);

  config_ptr config_;
};

} // namespace caf::net::web_socket
