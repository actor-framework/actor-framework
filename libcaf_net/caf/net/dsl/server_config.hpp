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
#include "caf/net/ssl/context.hpp"
#include "caf/net/tcp_accept_socket.hpp"

#include <cassert>
#include <cstdint>
#include <string>

namespace caf::net::dsl {

/// Meta programming utility.
template <class T>
struct server_config_tag {
  using type = T;
};

/// Wraps configuration parameters for starting clients.
class server_config {
public:
  /// Configuration for a server that creates the socket on demand.
  class lazy : public has_ctx {
  public:
    static constexpr std::string_view name = "lazy";

    lazy(uint16_t port, std::string bind_address)
      : port(port), bind_address(std::move(bind_address)) {
      // nop
    }

    /// The port number to bind to.
    uint16_t port = 0;

    /// The address to bind to.
    std::string bind_address;

    /// Whether to set `SO_REUSEADDR` on the socket.
    bool reuse_addr = true;
  };

  static constexpr auto lazy_v = server_config_tag<lazy>{};

  /// Configuration for a server that uses a user-provided socket.
  class socket : public has_ctx {
  public:
    static constexpr std::string_view name = "socket";

    explicit socket(tcp_accept_socket fd) : fd(fd) {
      // nop
    }

    ~socket() {
      if (fd != invalid_socket)
        close(fd);
    }

    /// The socket file descriptor to use.
    tcp_accept_socket fd;

    /// Returns the file descriptor and setting the `fd` member variable to the
    /// invalid socket.
    tcp_accept_socket take_fd() noexcept {
      auto result = fd;
      fd.id = invalid_socket_id;
      return result;
    }
  };

  static constexpr auto socket_v = server_config_tag<error>{};

  static constexpr auto fail_v = server_config_tag<error>{};

  template <class Base>
  class value : public config_impl<Base, lazy, socket> {
  public:
    using super = config_impl<Base, lazy, socket>;

    using super::super;

    template <class T, class From, class... Args>
    static auto make(server_config_tag<T>, const From& from, Args&&... args) {
      static_assert(std::is_constructible_v<T, Args...>);
      return make_counted<value>(from, std::in_place_type<T>,
                                 std::forward<Args>(args)...);
    }

    /// Configures how many reads we allow on a socket before returning to the
    /// event loop.
    size_t max_consecutive_reads
      = caf::defaults::middleman::max_consecutive_reads;

    /// Configures how many concurrent connections the server allows.
    size_t max_connections = defaults::net::max_connections.fallback;
  };
};

template <class Base>
using server_config_value = server_config::value<Base>;

template <class Base>
using server_config_ptr = intrusive_ptr<server_config_value<Base>>;

} // namespace caf::net::dsl
