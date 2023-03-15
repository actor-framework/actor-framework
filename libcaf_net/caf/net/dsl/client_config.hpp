// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/callback.hpp"
#include "caf/defaults.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/net/dsl/base.hpp"
#include "caf/net/dsl/config_base.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/ssl/connection.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/uri.hpp"

#include <cassert>
#include <cstdint>
#include <string>

namespace caf::net::dsl {

/// Meta programming utility.
template <class T>
struct client_config_tag {};

/// Simple type for storing host and port information for reaching a server.
struct server_address {
  /// The host name or IP address of the host.
  std::string host;

  /// The port to connect to.
  uint16_t port;
};

class client_config {
public:
  /// Configuration for a client that creates the socket on demand.
  class lazy {
  public:
    /// Type for holding a client address.
    using server_t = std::variant<server_address, uri>;

    lazy(std::string host, uint16_t port) {
      server = server_address{std::move(host), port};
    }

    explicit lazy(uri addr) {
      server = addr;
    }

    /// The address for reaching the server or an error.
    server_t server;

    /// SSL context for secure servers.
    std::shared_ptr<ssl::context> ctx;

    /// The delay between connection attempts.
    timespan retry_delay = std::chrono::seconds{1};

    /// The timeout when trying to connect.
    timespan connection_timeout = infinite;

    /// The maximum amount of retries.
    size_t max_retry_count = 0;

    /// Returns a function that, when called with a @ref stream_socket, calls
    /// `f` either with a new SSL connection from `ctx` or with the file the
    /// file descriptor if no SSL context is defined.
    template <class F>
    auto with_ctx(F&& f) {
      return [this, g = std::forward<F>(f)](stream_socket fd) mutable {
        using res_t = decltype(g(fd));
        if (ctx) {
          auto conn = ctx->new_connection(fd);
          if (conn)
            return g(*conn);
          else
            return res_t{std::move(conn.error())};
        } else
          return g(fd);
      };
    }
  };

  static constexpr auto lazy_v = client_config_tag<lazy>{};

  /// Configuration for a client that uses a user-provided socket.
  class socket {
  public:
    explicit socket(stream_socket fd) : fd(fd) {
      // nop
    }

    socket() = delete;

    socket(const socket&) = delete;

    socket& operator=(const socket&) = delete;

    socket(socket&& other) noexcept : fd(other.fd) {
      other.fd.id = invalid_socket_id;
    }

    socket& operator=(socket&& other) noexcept {
      using std::swap;
      swap(fd, other.fd);
      return *this;
    }

    ~socket() {
      if (fd != invalid_socket)
        close(fd);
    }

    /// The socket file descriptor to use.
    stream_socket fd;

    /// SSL context for secure servers.
    std::shared_ptr<ssl::context> ctx;

    /// Returns the file descriptor and setting the `fd` member variable to the
    /// invalid socket.
    stream_socket take_fd() noexcept {
      auto result = fd;
      fd.id = invalid_socket_id;
      return result;
    }
  };

  static constexpr auto socket_v = client_config_tag<socket>{};

  /// Configuration for a client that uses an already established SSL
  /// connection.
  class conn {
  public:
    explicit conn(ssl::connection st) : state(std::move(st)) {
      // nop
    }

    conn() = delete;

    conn(const conn&) = delete;

    conn& operator=(const conn&) = delete;

    conn(conn&&) noexcept = default;

    conn& operator=(conn&&) noexcept = default;

    ~conn() {
      if (state) {
        if (auto fd = state.fd(); fd != invalid_socket)
          close(fd);
      }
    }

    /// SSL state for the connection.
    ssl::connection state;
  };

  static constexpr auto conn_v = client_config_tag<conn>{};

  static constexpr auto fail_v = client_config_tag<error>{};

  template <class Base>
  class value : public Base {
  public:
    using super = Base;

    template <class Trait, class... Data>
    value(net::multiplexer* mpx, Trait trait, Data&&... arg)
      : super(mpx, std::move(trait)), data(std::forward<Data>(arg)...) {
      // nop
    }

    template <class... Data>
    explicit value(const value& other, Data&&... arg)
      : super(other), data(std::forward<Data>(arg)...) {
      // nop
    }

    std::variant<error, lazy, socket, conn> data;

    template <class T, class Trait, class... Args>
    static intrusive_ptr<value> make(client_config_tag<T>,
                                     net::multiplexer* mpx, Trait trait,
                                     Args&&... args) {
      return make_counted<value>(mpx, std::move(trait), std::in_place_type<T>,
                                 std::forward<Args>(args)...);
    }

    template <class F>
    auto visit(F&& f) {
      return std::visit([&](auto& arg) { return f(arg); }, data);
    }
  };
};

template <class Base>
using client_config_value = client_config::value<Base>;

template <class Base>
using client_config_ptr = intrusive_ptr<client_config_value<Base>>;

/// Creates a `fail_client_config` from another configuration object plus error.
template <class Base>
client_config_ptr<Base> to_fail_config(client_config_ptr<Base> ptr, error err) {
  using val_t = typename client_config::template value<Base>;
  return make_counted<val_t>(*ptr, std::move(err));
}

} // namespace caf::net::dsl
