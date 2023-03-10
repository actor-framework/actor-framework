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

/// The server config type enum class.
enum class client_config_type { lazy, socket, conn, fail };

/// Base class for client configuration objects.
template <class Trait>
class client_config : public config_base {
public:
  using trait_type = Trait;

  class lazy;
  class socket;
  class conn;
  class fail;

  friend class lazy;
  friend class socket;
  friend class conn;
  friend class fail;

  /// Virtual destructor.
  virtual ~client_config() = default;

  /// Returns the server configuration type.
  virtual client_config_type type() const noexcept = 0;

  /// The user-defined trait for configuration serialization.
  Trait trait;

private:
  /// Private constructor to enforce sealing.
  client_config(multiplexer* mpx, const Trait& trait)
    : config_base(mpx), trait(trait) {
    // nop
  }
};

/// Intrusive pointer type for server configurations.
template <class Trait>
using client_config_ptr = intrusive_ptr<client_config<Trait>>;

/// Simple type for storing host and port information for reaching a server.
struct client_config_server_address {
  /// The host name or IP address of the host.
  std::string host;

  /// The port to connect to.
  uint16_t port;
};

/// Configuration for a client that creates the socket on demand.
template <class Trait>
class client_config<Trait>::lazy final : public client_config<Trait> {
public:
  static constexpr auto type_token = client_config_type::lazy;

  using super = client_config;

  lazy(multiplexer* mpx, const Trait& trait, std::string host, uint16_t port)
    : super(mpx, trait) {
    server = client_config_server_address{std::move(host), port};
  }

  lazy(multiplexer* mpx, const Trait& trait, const uri& addr)
    : super(mpx, trait) {
    server = addr;
  }

  /// Returns the server configuration type.
  client_config_type type() const noexcept override {
    return type_token;
  }

  /// Type for holding a client address.
  using server_t = std::variant<client_config_server_address, uri>;

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
};

/// Configuration for a client that uses a user-provided socket.
template <class Trait>
class client_config<Trait>::socket final : public client_config<Trait> {
public:
  static constexpr auto type_token = client_config_type::socket;

  using super = client_config;

  socket(multiplexer* mpx, const Trait& trait) : super(mpx, trait) {
    // nop
  }

  socket(multiplexer* mpx, const Trait& trait, stream_socket fd)
    : super(mpx, trait), fd(fd) {
    // nop
  }

  ~socket() override {
    if (fd != invalid_socket)
      close(fd);
  }

  /// Returns the server configuration type.
  client_config_type type() const noexcept override {
    return type_token;
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

/// Configuration for a client that uses an already established SSL connection.
template <class Trait>
class client_config<Trait>::conn final : public client_config<Trait> {
public:
  static constexpr auto type_token = client_config_type::conn;

  using super = client_config;

  conn(multiplexer* mpx, const Trait& trait, ssl::connection state)
    : super(mpx, trait), state(std::move(state)) {
    // nop
  }

  ~conn() override {
    if (state) {
      if (auto fd = state.fd(); fd != invalid_socket)
        close(fd);
    }
  }

  /// Returns the server configuration type.
  client_config_type type() const noexcept override {
    return type_token;
  }

  /// SSL state for the connection.
  ssl::connection state;
};

/// Wraps an error that occurred earlier in the setup phase.
template <class Trait>
class client_config<Trait>::fail final : public client_config<Trait> {
public:
  static constexpr auto type_token = client_config_type::fail;

  using super = client_config;

  fail(multiplexer* mpx, const Trait& trait, error err)
    : super(mpx, trait), err(std::move(err)) {
    // nop
  }

  /// Returns the server configuration type.
  client_config_type type() const noexcept override {
    return type_token;
  }

  /// The forwarded error.
  error err;
};

/// Convenience alias for the `lazy` sub-type of @ref client_config.
template <class Trait>
using lazy_client_config = typename client_config<Trait>::lazy;

/// Convenience alias for the `socket` sub-type of @ref client_config.
template <class Trait>
using socket_client_config = typename client_config<Trait>::socket;

/// Convenience alias for the `conn` sub-type of @ref client_config.
template <class Trait>
using conn_client_config = typename client_config<Trait>::conn;

/// Convenience alias for the `fail` sub-type of @ref client_config.
template <class Trait>
using fail_client_config = typename client_config<Trait>::fail;

/// Calls a function object with the actual subtype of a client configuration
/// and returns its result.
template <class F, class Trait>
decltype(auto) visit(F&& f, client_config<Trait>& cfg) {
  auto type = cfg.type();
  switch (cfg.type()) {
    case client_config_type::lazy:
      return f(static_cast<lazy_client_config<Trait>&>(cfg));
    case client_config_type::socket:
      return f(static_cast<socket_client_config<Trait>&>(cfg));
    case client_config_type::conn:
      return f(static_cast<conn_client_config<Trait>&>(cfg));
    default:
      assert(type == client_config_type::fail);
      return f(static_cast<fail_client_config<Trait>&>(cfg));
  }
}

/// Calls a function object with the actual subtype of a client configuration
/// and returns its result.
template <class F, class Trait>
decltype(auto) visit(F&& f, const client_config<Trait>& cfg) {
  auto type = cfg.type();
  switch (cfg.type()) {
    case client_config_type::lazy:
      return f(static_cast<const lazy_client_config<Trait>&>(cfg));
    case client_config_type::socket:
      return f(static_cast<const socket_client_config<Trait>&>(cfg));
    case client_config_type::conn:
      return f(static_cast<const conn_client_config<Trait>&>(cfg));
    default:
      assert(type == client_config_type::fail);
      return f(static_cast<const fail_client_config<Trait>&>(cfg));
  }
}

/// Gets a pointer to a specific subtype of a client configuration.
template <class T, class Trait>
T* get_if(client_config<Trait>* config) {
  if (T::type_token == config->type())
    return static_cast<T*>(config);
  return nullptr;
}

/// Gets a pointer to a specific subtype of a client configuration.
template <class T, class Trait>
const T* get_if(const client_config<Trait>* config) {
  if (T::type_token == config->type())
    return static_cast<const T*>(config);
  return nullptr;
}

/// Creates a `fail_client_config` from another configuration object plus error.
template <class Trait>
auto to_fail_config(client_config_ptr<Trait> ptr, error err) {
  using impl_t = fail_client_config<Trait>;
  return make_counted<impl_t>(ptr->mpx, ptr->trait, std::move(err));
}

} // namespace caf::net::dsl
