// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/spsc_buffer.hpp"
#include "caf/detail/binary_flow_bridge.hpp"
#include "caf/detail/flow_connector.hpp"
#include "caf/disposable.hpp"
#include "caf/net/lp/framing.hpp"
#include "caf/net/tcp_stream_socket.hpp"
#include "caf/timespan.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <variant>

namespace caf::net::lp {

template <class>
class with_t;

/// Factory for the `with(...).connect(...).start(...)` DSL.
template <class Trait>
class connect_factory {
public:
  friend class with_t<Trait>;

  connect_factory(const connect_factory&) noexcept = delete;

  connect_factory& operator=(const connect_factory&) noexcept = delete;

  connect_factory(connect_factory&&) noexcept = default;

  connect_factory& operator=(connect_factory&&) noexcept = default;

  /// Starts a connection with the length-prefixing protocol.
  template <class OnStart>
  disposable start(OnStart on_start) {
    using input_res_t = typename Trait::input_resource;
    using output_res_t = typename Trait::output_resource;
    static_assert(std::is_invocable_v<OnStart, input_res_t, output_res_t>);
    switch (state_.index()) {
      case 1: { // config
        auto fd = try_connect(std::get<1>(state_));
        if (fd) {
          if (ctx_) {
            auto conn = ctx_->new_connection(*fd);
            if (conn)
              return do_start(std::move(*conn), on_start);
            if (do_on_error_)
              do_on_error_(conn.error());
            return {};
          }
          return do_start(*fd, on_start);
        }
        if (do_on_error_)
          do_on_error_(fd.error());
        return {};
      }
      case 2: { // stream_socket
        // Pass ownership of the stream socket.
        auto fd = std::get<2>(state_);
        state_ = none;
        return do_start(fd, on_start);
      }
      case 3: { // ssl::connection
        // Pass ownership of the SSL connection.
        auto conn = std::move(std::get<3>(state_));
        state_ = none;
        return do_start(std::move(conn), on_start);
      }
      case 4: // error
        if (do_on_error_)
          do_on_error_(std::get<4>(state_));
        return {};
      default:
        return {};
    }
  }

  /// Sets the retry delay for connection attempts.
  ///
  /// @param value The new retry delay.
  /// @returns a reference to this `connect_factory`.
  connect_factory& retry_delay(timespan value) {
    if (auto* cfg = std::get_if<config>(&state_))
      cfg->retry_delay = value;
    return *this;
  }

  /// Sets the connection timeout for connection attempts.
  ///
  /// @param value The new connection timeout.
  /// @returns a reference to this `connect_factory`.
  connect_factory& connection_timeout(timespan value) {
    if (auto* cfg = std::get_if<config>(&state_))
      cfg->connection_timeout = value;
    return *this;
  }

  /// Sets the maximum number of connection retry attempts.
  ///
  /// @param value The new maximum retry count.
  /// @returns a reference to this `connect_factory`.
  connect_factory& max_retry_count(size_t value) {
    if (auto* cfg = std::get_if<config>(&state_))
      cfg->max_retry_count = value;
    return *this;
  }

  /// Sets the callback for errors.
  /// @returns a reference to this `connect_factory`.
  template <class F>
  connect_factory& do_on_error(F callback) {
    do_on_error_ = std::move(callback);
    return *this;
  }

private:
  struct config {
    config(std::string address, uint16_t port)
      : address(std::move(address)), port(port) {
      // nop
    }

    std::string address;
    uint16_t port;
    timespan retry_delay = std::chrono::seconds{1};
    timespan connection_timeout = infinite;
    size_t max_retry_count = 0;
  };

  expected<tcp_stream_socket> try_connect(const config& cfg) {
    auto result = make_connected_tcp_stream_socket(cfg.address, cfg.port,
                                                   cfg.connection_timeout);
    if (result)
      return result;
    for (size_t i = 1; i <= cfg.max_retry_count; ++i) {
      std::this_thread::sleep_for(cfg.retry_delay);
      result = make_connected_tcp_stream_socket(cfg.address, cfg.port,
                                                cfg.connection_timeout);
      if (result)
        return result;
    }
    return result;
  }

  template <class Conn, class OnStart>
  disposable do_start(Conn conn, OnStart& on_start) {
    // s2a: socket-to-application (and a2s is the inverse).
    using input_t = typename Trait::input_type;
    using output_t = typename Trait::output_type;
    using transport_t = typename Conn::transport_type;
    auto [s2a_pull, s2a_push] = async::make_spsc_buffer_resource<input_t>();
    auto [a2s_pull, a2s_push] = async::make_spsc_buffer_resource<output_t>();
    auto fc = detail::flow_connector<Trait>::make_trivial(std::move(a2s_pull),
                                                          std::move(s2a_push));
    auto bridge = detail::binary_flow_bridge<Trait>::make(mpx_, std::move(fc));
    auto bridge_ptr = bridge.get();
    auto impl = framing::make(std::move(bridge));
    auto transport = transport_t::make(std::move(conn), std::move(impl));
    auto ptr = socket_manager::make(mpx_, std::move(transport));
    bridge_ptr->self_ref(ptr->as_disposable());
    mpx_->start(ptr);
    on_start(std::move(s2a_pull), std::move(a2s_push));
    return disposable{std::move(ptr)};
  }

  explicit connect_factory(multiplexer* mpx) : mpx_(mpx) {
    // nop
  }

  connect_factory(multiplexer* mpx, error err)
    : mpx_(mpx), state_(std::move(err)) {
    // nop
  }

  /// Initializes the connect factory to connect to the given TCP `host` and
  /// `port`.
  ///
  /// @param host The hostname or IP address to connect to.
  /// @param port The port number to connect to.
  void init(std::string host, uint16_t port) {
    state_ = config{std::move(host), port};
  }

  /// Initializes the connect factory to connect to the given TCP `socket`.
  ///
  /// @param fd The TCP socket to connect.
  void init(stream_socket fd) {
    state_ = fd;
  }

  /// Initializes the connect factory to connect to the given TCP `socket`.
  ///
  /// @param conn The SSL connection object.
  void init(ssl::connection conn) {
    state_ = std::move(conn);
  }

  /// Initializes the connect factory with an error.
  ///
  /// @param err The error to be later forwarded to the `do_on_error_` handler.
  void init(error err) {
    state_ = std::move(err);
  }

  void set_ssl(ssl::context ctx) {
    ctx_ = std::make_shared<ssl::context>(std::move(ctx));
  }

  /// Pointer to multiplexer that runs the protocol stack.
  multiplexer* mpx_;

  /// Callback for errors.
  std::function<void(const error&)> do_on_error_;

  /// Configures the maximum number of concurrent connections.
  size_t max_connections_ = defaults::net::max_connections.fallback;

  /// User-defined state for getting things up and running.
  std::variant<none_t, config, stream_socket, ssl::connection, error> state_;

  /// Pointer to the (optional) SSL context.
  std::shared_ptr<ssl::context> ctx_;
};

} // namespace caf::net::lp
