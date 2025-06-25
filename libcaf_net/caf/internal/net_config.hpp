// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/ssl/connection.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/ssl/tcp_acceptor.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/callback.hpp"
#include "caf/defaults.hpp"
#include "caf/disposable.hpp"
#include "caf/none.hpp"
#include "caf/uri.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <variant>

namespace caf::internal {

class net_config {
public:
  class server_config {
  public:
    class lazy {
    public:
      lazy(uint16_t port, std::string bind_address, bool reuse)
        : port(port), bind_address(std::move(bind_address)), reuse_addr(reuse) {
        // nop
      }

      /// The port number to bind to.
      uint16_t port = 0;

      /// The address to bind to.
      std::string bind_address;

      /// Whether to set `SO_REUSEADDR` on the socket.
      bool reuse_addr = true;
    };

    /// Configuration for a server that uses a user-provided socket.
    class socket {
    public:
      explicit socket(net::tcp_accept_socket fd) : fd(fd) {
        // nop
      }

      ~socket() {
        if (fd != net::invalid_socket) {
          net::close(fd);
        }
      }

      /// The socket file descriptor to use.
      net::tcp_accept_socket fd;

      /// Returns the file descriptor and setting the `fd` member variable to
      /// the invalid socket.
      net::tcp_accept_socket take_fd() noexcept {
        auto result = fd;
        fd.id = net::invalid_socket_id;
        return result;
      }
    };

    void assign(uint16_t port, std::string&& bind_address, bool reuse_addr) {
      value.emplace<lazy>(port, std::move(bind_address), reuse_addr);
    }

    void assign(net::tcp_accept_socket fd) {
      value.emplace<socket>(fd);
    }

    using value_t = std::variant<none_t, lazy, socket>;

    value_t value;
  };

  class client_config {
  public:
    /// Simple type for storing host and port information for reaching a server.
    struct server_address {
      /// The host name or IP address of the host.
      std::string host;

      /// The port to connect to.
      uint16_t port;
    };

    class lazy {
    public:
      /// Type for holding a client address.
      using server_t = std::variant<server_address, uri>;

      lazy(std::string host, uint16_t port)
        : server(server_address{std::move(host), port}) {
        // nop
      }

      explicit lazy(uri addr) : server(std::move(addr)) {
        // nop
      }

      /// The address for reaching the server or an error.
      server_t server;
    };

    class socket {
    public:
      explicit socket(net::stream_socket fd) : fd(fd) {
        // nop
      }

      socket() = delete;

      socket(const socket&) = delete;

      socket& operator=(const socket&) = delete;

      socket(socket&& other) noexcept : fd(other.fd) {
        other.fd.id = net::invalid_socket_id;
      }

      socket& operator=(socket&& other) noexcept {
        using std::swap;
        swap(fd, other.fd);
        return *this;
      }

      ~socket() {
        if (fd != net::invalid_socket) {
          net::close(fd);
        }
      }

      /// The socket file descriptor to use.
      net::stream_socket fd;

      /// Returns the file descriptor and setting the `fd` member variable to
      /// the invalid socket.
      net::stream_socket take_fd() noexcept {
        auto result = fd;
        fd.id = net::invalid_socket_id;
        return result;
      }
    };

    class conn {
    public:
      explicit conn(net::ssl::connection st) : state(std::move(st)) {
        // nop
      }

      conn() = delete;

      conn(const conn&) = delete;

      conn& operator=(const conn&) = delete;

      conn(conn&&) noexcept = default;

      conn& operator=(conn&&) noexcept = default;

      ~conn() {
        if (state) {
          if (auto fd = state.fd(); fd != net::invalid_socket)
            net::close(fd);
        }
      }

      /// SSL state for the connection.
      net::ssl::connection state;
    };

    void assign(std::string&& host, uint16_t port) {
      value.emplace<lazy>(std::move(host), port);
    }

    void assign(uri&& endpoint) {
      value.emplace<lazy>(std::move(endpoint));
    }

    void assign(net::stream_socket fd) {
      value.emplace<socket>(fd);
    }

    void assign(net::ssl::connection&& hdl) {
      value.emplace<conn>(std::move(hdl));
    }

    using value_t = std::variant<none_t, lazy, socket, conn>;

    value_t value;
  };

  using on_error_callback = unique_callback_ptr<void(const error&)>;

  explicit net_config(net::multiplexer* parent) : mpx(parent) {
    // nop
  }

  virtual ~net_config();

  net_config(const net_config&) = delete;

  net_config& operator=(const net_config&) = delete;

  // common state

  net::multiplexer* mpx;

  std::shared_ptr<net::ssl::context> ctx;

  on_error_callback on_error;

  error err;

  // state for servers

  server_config server;

  size_t max_connections = defaults::net::max_connections.fallback;

  size_t max_consecutive_reads = defaults::net::max_consecutive_reads.fallback;

  /// Store actors that the server should monitor.
  std::vector<strong_actor_ptr> monitored_actors;

  void do_monitor(strong_actor_ptr ptr) {
    if (ptr) {
      monitored_actors.push_back(std::move(ptr));
      return;
    }
    err = make_error(sec::logic_error,
                     "cannot monitor an invalid actor handle");
  }

  virtual expected<disposable> start_server_impl(net::ssl::tcp_acceptor&) = 0;

  virtual expected<disposable> start_server_impl(net::tcp_accept_socket) = 0;

  expected<disposable> start_server(none_t) {
    return make_error(caf::sec::logic_error,
                      "invalid WebSocket server configuration");
  }

  expected<disposable> start_server(server_config::socket& cfg) {
    if (ctx) {
      net::ssl::tcp_acceptor acc{cfg.take_fd(), ctx};
      return start_server_impl(acc);
    }
    auto fd = cfg.take_fd();
    return start_server_impl(fd);
  }

  expected<disposable> start_server(server_config::lazy& cfg) {
    auto maybe_fd = net::make_tcp_accept_socket(cfg.port,
                                                std::move(cfg.bind_address),
                                                cfg.reuse_addr);
    if (!maybe_fd) {
      return maybe_fd.error();
    }
    server_config::socket sub_cfg{*maybe_fd};
    return start_server(sub_cfg);
  }

  expected<disposable> start_server() {
    auto fn = [this](auto& val) -> expected<disposable> {
      return start_server(val);
    };
    return std::visit(fn, server.value);
  }

  // state for clients

  /// SSL context factory for lazy loading SSL on demand.
  unique_callback_ptr<expected<net::ssl::context>()> context_factory
    = make_type_erased_callback(default_ctx_factory);

  /// The delay between connection attempts.
  timespan retry_delay = timespan{1'000'000}; // 1s

  /// The timeout when trying to connect.
  timespan connection_timeout = infinite;

  /// The maximum amount of retries.
  size_t max_retry_count = 0;

  client_config client;

  virtual expected<disposable> start_client_impl(net::ssl::connection&) = 0;

  virtual expected<disposable> start_client_impl(net::stream_socket) = 0;

  expected<disposable> start_client(none_t) {
    return make_error(caf::sec::logic_error,
                      "invalid WebSocket client configuration");
  }

  expected<disposable> start_client(client_config::conn& cfg) {
    return start_client_impl(cfg.state);
  }

  expected<disposable> start_client(client_config::socket& cfg) {
    if (ctx) {
      auto conn = ctx->new_connection(cfg.take_fd());
      if (!conn) {
        return conn.error();
      }
      return start_client_impl(*conn);
    }
    auto fd = cfg.take_fd();
    return start_client_impl(fd);
  }

  expected<disposable> start_client(std::string& host, uint16_t port) {
    auto maybe_fd = detail::tcp_try_connect(std::move(host), port,
                                            connection_timeout, max_retry_count,
                                            retry_delay);
    if (!maybe_fd) {
      return maybe_fd.error();
    }
    client_config::socket sub_cfg{*maybe_fd};
    return start_client(sub_cfg);
  }

  virtual expected<disposable> start_client_impl(uri& endpoint) = 0;

  expected<disposable> start_client(client_config::lazy& cfg) {
    auto fn = [this](auto& val) {
      using val_t = std::decay_t<decltype(val)>;
      if constexpr (std::is_same_v<val_t, uri>) {
        return start_client_impl(val);
      } else {
        return start_client(val.host, val.port);
      }
    };
    return std::visit(fn, cfg.server);
  }

  expected<disposable> start_client() {
    auto fn = [this](auto& val) -> expected<disposable> {
      return start_client(val);
    };
    return std::visit(fn, client.value);
  }

private:
  static expected<net::ssl::context> default_ctx_factory() {
    return net::ssl::context::make_client(net::ssl::tls::v1_2);
  }
};

} // namespace caf::internal
