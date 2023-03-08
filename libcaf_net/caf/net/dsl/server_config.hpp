// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/callback.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/plain_ref_counted.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/net/dsl/has_trait.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/tcp_accept_socket.hpp"

#include <cassert>
#include <cstdint>
#include <string>

namespace caf::net::dsl {

/// The server config type enum class.
enum class server_config_type { lazy, socket };

/// Base class for server configuration objects.
template <class Trait>
class server_config : public detail::plain_ref_counted {
public:
  class lazy;
  class socket;

  friend class lazy;
  friend class socket;

  server_config(const server_config&) = delete;

  server_config& operator=(const server_config&) = delete;

  /// Virtual destructor.
  virtual ~server_config() = default;

  /// Returns the server configuration type.
  virtual server_config_type type() const noexcept = 0;

  /// The pointer to the @ref multiplexer for running the server.
  multiplexer* mpx;

  /// The user-defined trait for configuration serialization.
  Trait trait;

  /// SSL context for secure servers.
  std::shared_ptr<ssl::context> ctx;

  /// User-defined callback for errors.
  shared_callback_ptr<void(const error&)> on_error;

  /// Configures the maximum number of concurrent connections.
  size_t max_connections = defaults::net::max_connections.fallback;

  /// Calls `on_error` if non-null.
  void call_on_error(const error& what) {
    if (on_error)
      (*on_error)(what);
  }

  friend void intrusive_ptr_add_ref(const server_config* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const server_config* ptr) noexcept {
    ptr->deref();
  }

private:
  /// Private constructor to enforce sealing.
  server_config(multiplexer* mpx, const Trait& trait) : mpx(mpx), trait(trait) {
    // nop
  }
};

/// Intrusive pointer type for server configurations.
template <class Trait>
using server_config_ptr = intrusive_ptr<server_config<Trait>>;

/// Configuration for a server that creates the socket on demand.
template <class Trait>
class server_config<Trait>::lazy final : public server_config<Trait> {
public:
  static constexpr auto type_token = server_config_type::lazy;

  using super = server_config;

  lazy(multiplexer* mpx, const Trait& trait, uint16_t port,
       std::string bind_address)
    : super(mpx, trait), port(port), bind_address(std::move(bind_address)) {
    // nop
  }

  /// Returns the server configuration type.
  server_config_type type() const noexcept override {
    return type_token;
  }

  /// The port number to bind to.
  uint16_t port = 0;

  /// The address to bind to.
  std::string bind_address;

  /// Whether to set `SO_REUSEADDR` on the socket.
  bool reuse_addr = true;
};

/// Configuration for a server that uses a user-provided socket.
template <class Trait>
class server_config<Trait>::socket final : public server_config<Trait> {
public:
  static constexpr auto type_token = server_config_type::socket;

  using super = server_config;

  socket(multiplexer* mpx, const Trait& trait, tcp_accept_socket fd)
    : super(mpx, trait), fd(fd) {
    // nop
  }

  ~socket() override {
    if (fd != invalid_socket)
      close(fd);
  }

  /// Returns the server configuration type.
  server_config_type type() const noexcept override {
    return type_token;
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

/// Convenience alias for the `lazy` sub-type of @ref server_config.
template <class Trait>
using lazy_server_config = typename server_config<Trait>::lazy;

/// Convenience alias for the `socket` sub-type of @ref server_config.
template <class Trait>
using socket_server_config = typename server_config<Trait>::socket;

/// Calls a function object with the actual subtype of a server configuration
/// and returns its result.
template <class F, class Trait>
decltype(auto) visit(F&& f, server_config<Trait>& cfg) {
  auto type = cfg.type();
  if (cfg.type() == server_config_type::lazy)
    return f(static_cast<lazy_server_config<Trait>&>(cfg));
  assert(type == server_config_type::socket);
  return f(static_cast<socket_server_config<Trait>&>(cfg));
}

/// Calls a function object with the actual subtype of a server configuration.
template <class F, class Trait>
decltype(auto) visit(F&& f, const server_config<Trait>& cfg) {
  auto type = cfg.type();
  if (cfg.type() == server_config_type::lazy)
    return f(static_cast<const lazy_server_config<Trait>&>(cfg));
  assert(type == server_config_type::socket);
  return f(static_cast<const socket_server_config<Trait>&>(cfg));
}

/// Gets a pointer to a specific subtype of a server configuration.
template <class T, class Trait>
T* get_if(server_config<Trait>* cfg) {
  if (T::type_token == cfg->type())
    return static_cast<T*>(cfg);
  return nullptr;
}

/// Gets a pointer to a specific subtype of a server configuration.
template <class T, class Trait>
const T* get_if(const server_config<Trait>* cfg) {
  if (T::type_token == cfg->type())
    return static_cast<const T*>(cfg);
  return nullptr;
}

} // namespace caf::net::dsl
