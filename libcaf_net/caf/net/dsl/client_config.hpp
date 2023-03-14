// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/callback.hpp"
#include "caf/defaults.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/net/dsl/base.hpp"
#include "caf/net/dsl/config_base.hpp"
#include "caf/net/dsl/fwd.hpp"
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

/// Meta programming utility for `as_base_ptr()`.
struct client_config_tag {};

/// Meta programming utility for `client_config<Base>::make()`.
template <class T>
struct client_config_token {};

/// Base class for client configuration objects.
template <class Base>
class client_config : public Base, public client_config_tag {
public:
  friend class lazy_client_config<Base>;
  friend class socket_client_config<Base>;
  friend class conn_client_config<Base>;
  friend class fail_client_config<Base>;

  using lazy = lazy_client_config<Base>;
  using socket = socket_client_config<Base>;
  using conn = conn_client_config<Base>;
  using fail = fail_client_config<Base>;

  /// Anchor type for meta programming.
  using base_type = client_config;

  client_config(client_config&&) = default;

  client_config(const client_config&) = default;

  /// Virtual destructor.
  virtual ~client_config() = default;

  /// Returns the server configuration type.
  virtual client_config_type type() const noexcept = 0;

  template <class Token, class... Ts>
  static auto make(client_config_token<Token>, Ts&&... xs);

private:
  /// Private constructor to enforce sealing.
  template <class... Ts>
  explicit client_config(multiplexer* mpx, Ts&&... xs)
    : Base(mpx, std::forward<Ts>(xs)...) {
    // nop
  }
};

#define CAF_NET_DSL_ADD_CLIENT_TOKEN(type)                                     \
  struct client_config_bind_##type {                                           \
    template <class Base>                                                      \
    using bind = typename client_config<Base>::type;                           \
  };                                                                           \
  static constexpr auto client_config_##type##_v                               \
    = client_config_token<client_config_bind_##type> {}

/// Compile-time constant for `client_config::lazy`.
CAF_NET_DSL_ADD_CLIENT_TOKEN(lazy);

/// Compile-time constant for `client_config::socket`.
CAF_NET_DSL_ADD_CLIENT_TOKEN(socket);

/// Compile-time constant for `client_config::conn`.
CAF_NET_DSL_ADD_CLIENT_TOKEN(conn);

/// Compile-time constant for `client_config::fail`.
CAF_NET_DSL_ADD_CLIENT_TOKEN(fail);

/// Intrusive pointer type for server configurations.
template <class Base>
using client_config_ptr = intrusive_ptr<client_config<Base>>;

/// Simple type for storing host and port information for reaching a server.
struct client_config_server_address {
  /// The host name or IP address of the host.
  std::string host;

  /// The port to connect to.
  uint16_t port;
};

/// Configuration for a client that creates the socket on demand.
template <class Base>
class lazy_client_config final : public client_config<Base> {
public:
  static constexpr auto type_token = client_config_type::lazy;

  using super = client_config<Base>;

  template <class... Ts>
  lazy_client_config(std::string host, uint16_t port, multiplexer* mpx,
                     Ts&&... xs)
    : super(mpx, std::forward<Ts>(xs)...) {
    server = client_config_server_address{std::move(host), port};
  }

  template <class... Ts>
  lazy_client_config(const uri& addr, multiplexer* mpx, Ts&&... xs)
    : super(mpx, std::forward<Ts>(xs)...) {
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
template <class Base>
class socket_client_config final : public client_config<Base> {
public:
  static constexpr auto type_token = client_config_type::socket;

  using super = client_config<Base>;

  template <class... Ts>
  socket_client_config(stream_socket fd, multiplexer* mpx, Ts&&... xs)
    : super(mpx, std::forward<Ts>(xs)...), fd(fd) {
    // nop
  }

  ~socket_client_config() override {
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
template <class Base>
class conn_client_config final : public client_config<Base> {
public:
  static constexpr auto type_token = client_config_type::conn;

  using super = client_config<Base>;

  template <class... Ts>
  conn_client_config(ssl::connection state, multiplexer* mpx, Ts&&... xs)
    : super(mpx, std::forward<Ts>(xs)...), state(std::move(state)) {
    // nop
  }

  ~conn_client_config() override {
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
template <class Base>
class fail_client_config final : public client_config<Base> {
public:
  static constexpr auto type_token = client_config_type::fail;

  using super = client_config<Base>;

  template <class... Ts>
  fail_client_config(error err, multiplexer* mpx, Ts&&... xs)
    : super(mpx, std::forward<Ts>(xs)...), err(std::move(err)) {
    // nop
  }

  fail_client_config(error err, const super& other)
    : super(other), err(std::move(err)) {
    // nop
  }

  /// Returns the server configuration type.
  client_config_type type() const noexcept override {
    return type_token;
  }

  /// The forwarded error.
  error err;
};

/// Calls a function object with the actual subtype of a client configuration
/// and returns its result.
template <class F, class Base>
decltype(auto) visit(F&& f, client_config<Base>& cfg) {
  auto type = cfg.type();
  switch (cfg.type()) {
    case client_config_type::lazy:
      return f(static_cast<lazy_client_config<Base>&>(cfg));
    case client_config_type::socket:
      return f(static_cast<socket_client_config<Base>&>(cfg));
    case client_config_type::conn:
      return f(static_cast<conn_client_config<Base>&>(cfg));
    default:
      assert(type == client_config_type::fail);
      return f(static_cast<fail_client_config<Base>&>(cfg));
  }
}

/// Calls a function object with the actual subtype of a client configuration
/// and returns its result.
template <class F, class Base>
decltype(auto) visit(F&& f, const client_config<Base>& cfg) {
  auto type = cfg.type();
  switch (cfg.type()) {
    case client_config_type::lazy:
      return f(static_cast<const lazy_client_config<Base>&>(cfg));
    case client_config_type::socket:
      return f(static_cast<const socket_client_config<Base>&>(cfg));
    case client_config_type::conn:
      return f(static_cast<const conn_client_config<Base>&>(cfg));
    default:
      assert(type == client_config_type::fail);
      return f(static_cast<const fail_client_config<Base>&>(cfg));
  }
}

/// Gets a pointer to a specific subtype of a client configuration.
template <class T, class Base>
T* get_if(client_config<Base>* config) {
  if (T::type_token == config->type())
    return static_cast<T*>(config);
  return nullptr;
}

/// Gets a pointer to a specific subtype of a client configuration.
template <class T, class Base>
const T* get_if(const client_config<Base>* config) {
  if (T::type_token == config->type())
    return static_cast<const T*>(config);
  return nullptr;
}

/// Creates a `fail_client_config` from another configuration object plus error.
template <class Base>
auto to_fail_config(client_config_ptr<Base> ptr, error err) {
  using impl_t = fail_client_config<Base>;
  return make_counted<impl_t>(std::move(err), *ptr);
}

/// Returns the pointer as a pointer to the `client_config` base type.
template <class T>
auto as_base_ptr(
  intrusive_ptr<T> ptr,
  std::enable_if_t<std::is_base_of_v<client_config_tag, T>>* = nullptr) {
  return std::move(ptr).template upcast<typename T::base_type>();
}

template <class Base>
template <class Token, class... Ts>
auto client_config<Base>::make(client_config_token<Token>, Ts&&... xs) {
  using type_t = typename Token::template bind<Base>;
  return make_counted<type_t>(std::forward<Ts>(xs)...);
}

} // namespace caf::net::dsl
