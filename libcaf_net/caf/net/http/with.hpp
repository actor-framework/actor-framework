// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/http/request.hpp"
#include "caf/net/http/route.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/tcp_accept_socket.hpp"

#include "caf/actor_cast.hpp"
#include "caf/callback.hpp"
#include "caf/fwd.hpp"

#include <cstdint>

namespace caf::net::http {

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

    using push_t = async::producer_resource<request>;

    using pull_t = async::consumer_resource<request>;

    ~server();

    /// Sets the maximum request size to @p value.
    [[nodiscard]] server&& max_request_size(size_t value) &&;

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

    /// Adds a new route to the HTTP server.
    /// @param path The path on this server for the new route.
    /// @param f The function object for handling requests on the new route.
    /// @return a reference to `*this`.
    template <class F>
    [[nodiscard]] server&& route(std::string path, F f) && {
      auto new_route = make_route(std::move(path), std::move(f));
      add_route(new_route);
      return std::move(*this);
    }

    /// Adds a new route to the HTTP server.
    /// @param path The path on this server for the new route.
    /// @param method The allowed HTTP method on the new route.
    /// @param f The function object for handling requests on the new route.
    /// @return a reference to `*this`.
    template <class F>
    [[nodiscard]] server&&
    route(std::string path, http::method method, F f) && {
      auto new_route = make_route(std::move(path), method, std::move(f));
      add_route(new_route);
      return std::move(*this);
    }

    /// Starts a server that makes HTTP requests without a fixed route available
    /// to an observer.
    template <class OnStart>
    [[nodiscard]] expected<disposable> start(OnStart on_start) && {
      static_assert(std::is_invocable_v<OnStart, pull_t>);
      auto [pull, push] = async::make_spsc_buffer_resource<request>();
      auto res = do_start(std::move(push));
      if (res) {
        on_start(std::move(pull));
      }
      return res;
    }

    /// Starts a server that only serves the fixed routes.
    [[nodiscard]] expected<disposable> start() && {
      return do_start(push_t{});
    }

  private:
    explicit server(config_ptr&& cfg) noexcept;

    void do_monitor(strong_actor_ptr ptr);

    void add_route(expected<route_ptr>& new_route);

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

    /// Add an additional HTTP header field to the request.
    /// @param name The name of the new field.
    /// @param value The value of the new field.
    /// @returns a reference to this `client`.
    [[nodiscard]] client&& add_header_field(std::string name,
                                            std::string value) &&;

    /// Add an additional HTTP header fields to the request.
    /// @param kv_map A container of key-value pairs to inserta as fields.
    /// @returns a reference to this `client`.
    template <class KeyValueMap>
    [[nodiscard]] client&& add_header_fields(KeyValueMap&& kv_map) && {
      for (auto&& [key, value] : std::forward<KeyValueMap>(kv_map)) {
        add_header_field(std::forward<decltype(key)>(key),
                         std::forward<decltype(value)>(value));
      }
      return std::move(*this);
    }

    /// Sends an HTTP GET message.
    [[nodiscard]] expected<std::pair<async::future<response>, disposable>>
    get() {
      return request(http::method::get);
    }

    /// Sends an HTTP HEAD message.
    [[nodiscard]] expected<std::pair<async::future<response>, disposable>>
    head() {
      return request(http::method::head);
    }

    /// Sends an HTTP POST message.
    [[nodiscard]] expected<std::pair<async::future<response>, disposable>>
    post(std::string_view payload) {
      return request(http::method::post, payload);
    }

    /// Sends an HTTP PUT message.
    [[nodiscard]] expected<std::pair<async::future<response>, disposable>>
    put(std::string_view payload) {
      return request(http::method::put, payload);
    }

    /// Sends an HTTP DELETE message.
    [[nodiscard]] expected<std::pair<async::future<response>, disposable>>
    del() {
      return request(http::method::del);
    }

    /// Sends an HTTP CONNECT message.
    [[nodiscard]] expected<std::pair<async::future<response>, disposable>>
    connect() {
      return request(http::method::connect);
    }

    /// Sends an HTTP OPTIONS message.
    [[nodiscard]] expected<std::pair<async::future<response>, disposable>>
    options(std::string_view payload) {
      return request(http::method::options, payload);
    }

    /// Sends an HTTP TRACE message.
    [[nodiscard]] expected<std::pair<async::future<response>, disposable>>
    trace(std::string_view payload) {
      return request(http::method::trace, payload);
    }

    /// Utility function to make a request with given parameters.
    /// @param method the HTTP method to send.
    /// @param payload optional payload to be included in the request.
    /// @returns an expected pair of the future response and disposable.
    [[nodiscard]] expected<std::pair<async::future<response>, disposable>>
    request(http::method method, std::string_view payload = std::string_view{});

    /// @copydoc request
    expected<std::pair<async::future<response>, disposable>>
    request(http::method method, const_byte_span payload);

  private:
    explicit client(config_ptr&& cfg) noexcept;

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
  [[nodiscard]] with_t&& context_factory(F&& factory) && {
    static_assert(std::is_same_v<decltype(factory()), expected<ssl::context>>);
    set_context_factory(std::forward<F>(factory));
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

  /// Creates a `client` object for the given TCP `endpoint`.
  /// @param endpoint The endpoint of the TCP server to connect to.
  /// @returns a `client` object initialized with the given parameters.
  [[nodiscard]] client connect(uri endpoint) &&;

  /// Creates a `client` object for the given TCP `endpoint`.
  /// @param endpoint The endpoint of the TCP server to connect to.
  /// @returns a `client` object initialized with the given parameters.
  [[nodiscard]] client connect(expected<uri> endpoint) &&;

private:
  using on_error_callback = unique_callback_ptr<void(const error&)>;

  void set_on_error(on_error_callback ptr);

  void set_context_factory(std::function<expected<ssl::context>()> fn);

  config_ptr config_;
};

} // namespace caf::net::http
