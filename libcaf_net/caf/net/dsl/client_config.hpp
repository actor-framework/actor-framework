// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/dsl/base.hpp"
#include "caf/net/dsl/config_base.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/ssl/connection.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/stream_socket.hpp"

#include "caf/callback.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/uri.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <utility>

namespace caf::net::dsl {

/// Meta programming utility.
template <class T>
struct client_config_tag {
  using type = T;
};

/// Simple type for storing host and port information for reaching a server.
struct server_address {
  /// The host name or IP address of the host.
  std::string host;

  /// The port to connect to.
  uint16_t port;
};

/// Wraps configuration parameters for starting clients.
class CAF_NET_EXPORT client_config {
public:
  virtual ~client_config();

  /// Configuration for a client that creates the socket on demand.
  class CAF_NET_EXPORT lazy : public has_make_ctx {
  public:
    /// Type for holding a client address.
    using server_t = std::variant<server_address, uri>;

    static constexpr std::string_view name = "lazy";

    lazy(std::string host, uint16_t port)
      : server(server_address{std::move(host), port}) {
      // nop
    }

    explicit lazy(const uri& addr) : server(addr) {
      // nop
    }

    /// The address for reaching the server or an error.
    server_t server;

    /// The delay between connection attempts.
    timespan retry_delay = std::chrono::seconds{1};

    /// The timeout when trying to connect.
    timespan connection_timeout = infinite;

    /// The maximum amount of retries.
    size_t max_retry_count = 0;
  };

  using lazy_t = client_config_tag<lazy>;

  static constexpr auto lazy_v = lazy_t{};

  /// Configuration for a client that uses a user-provided socket.
  class CAF_NET_EXPORT socket : public has_make_ctx {
  public:
    static constexpr std::string_view name = "socket";

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

    /// Returns the file descriptor and setting the `fd` member variable to the
    /// invalid socket.
    stream_socket take_fd() noexcept {
      auto result = fd;
      fd.id = invalid_socket_id;
      return result;
    }
  };

  using socket_t = client_config_tag<socket>;

  static constexpr auto socket_v = socket_t{};

  /// Configuration for a client that uses an already established SSL
  /// connection.
  class CAF_NET_EXPORT conn {
  public:
    static constexpr std::string_view name = "conn";

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

  using conn_t = client_config_tag<conn>;

  static constexpr auto conn_v = conn_t{};

  using fail_t = client_config_tag<error>;

  static constexpr auto fail_v = fail_t{};

  using value = config_impl<lazy, socket, conn>;
};

using client_config_value = client_config::value;

} // namespace caf::net::dsl
