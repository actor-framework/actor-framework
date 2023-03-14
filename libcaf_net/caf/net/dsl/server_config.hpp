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

/// The server config type enum class.
enum class server_config_type { lazy, socket, fail };

/// Empty tag type for meta programming.
struct server_config_tag {};

/// Base class for server configuration objects.
template <class Base>
class server_config : public Base, public server_config_tag {
public:
  using trait_type = typename Base::trait_type;

  class lazy;
  class socket;
  class fail;

  friend class lazy;
  friend class socket;
  friend class fail;

  /// Anchor type for meta programming.
  using base_type = server_config;

  server_config(server_config&&) = default;

  server_config(const server_config&) = default;

  /// Returns the server configuration type.
  virtual server_config_type type() const noexcept = 0;

  /// SSL context for secure servers.
  std::shared_ptr<ssl::context> ctx;

  /// Limits how many connections the server allows concurrently.
  size_t max_connections = defaults::net::max_connections.fallback;

  /// Limits how many times the server may read from the socket before allowing
  /// others to read.
  size_t max_consecutive_reads = defaults::middleman::max_consecutive_reads;

private:
  /// Private constructor to enforce sealing.
  template <class... Ts>
  explicit server_config(multiplexer* mpx, Ts&&... xs)
    : Base(mpx, std::forward<Ts>(xs)...) {
    // nop
  }
};

/// Intrusive pointer type for server configurations.
template <class Base>
using server_config_ptr = intrusive_ptr<server_config<Base>>;

/// Configuration for a server that creates the socket on demand.
template <class Base>
class server_config<Base>::lazy final : public server_config<Base> {
public:
  static constexpr auto type_token = server_config_type::lazy;

  using super = server_config;

  template <class... Ts>
  lazy(uint16_t port, std::string bind_address, multiplexer* mpx, Ts&&... xs)
    : super(mpx, std::forward<Ts>(xs)...),
      port(port),
      bind_address(std::move(bind_address)) {
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
template <class Base>
class server_config<Base>::socket final : public server_config<Base> {
public:
  static constexpr auto type_token = server_config_type::socket;

  using super = server_config;

  template <class... Ts>
  socket(tcp_accept_socket fd, multiplexer* mpx, Ts&&... xs)
    : super(mpx, std::forward<Ts>(xs)...), fd(fd) {
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

/// Wraps an error that occurred earlier in the setup phase.
template <class Base>
class server_config<Base>::fail final : public server_config<Base> {
public:
  static constexpr auto type_token = server_config_type::fail;

  using super = server_config;

  template <class... Ts>
  fail(error err, multiplexer* mpx, Ts&&... xs)
    : super(mpx, std::forward<Ts>(xs)...), err(std::move(err)) {
    // nop
  }

  fail(error err, const super& other) : super(other), err(std::move(err)) {
    // nop
  }

  /// Returns the server configuration type.
  server_config_type type() const noexcept override {
    return type_token;
  }

  /// The forwarded error.
  error err;
};

/// Convenience alias for the `lazy` sub-type of @ref server_config.
template <class Base>
using lazy_server_config = typename server_config<Base>::lazy;

/// Convenience alias for the `socket` sub-type of @ref server_config.
template <class Base>
using socket_server_config = typename server_config<Base>::socket;

/// Convenience alias for the `fail` sub-type of @ref server_config.
template <class Base>
using fail_server_config = typename server_config<Base>::fail;

/// Calls a function object with the actual subtype of a server configuration
/// and returns its result.
template <class F, class Base>
decltype(auto) visit(F&& f, server_config<Base>& cfg) {
  auto type = cfg.type();
  switch (cfg.type()) {
    case server_config_type::lazy:
      return f(static_cast<lazy_server_config<Base>&>(cfg));
    case server_config_type::socket:
      return f(static_cast<socket_server_config<Base>&>(cfg));
    default:
      assert(type == server_config_type::fail);
      return f(static_cast<fail_server_config<Base>&>(cfg));
  }
}

/// Calls a function object with the actual subtype of a server configuration.
template <class F, class Base>
decltype(auto) visit(F&& f, const server_config<Base>& cfg) {
  auto type = cfg.type();
  switch (cfg.type()) {
    case server_config_type::lazy:
      return f(static_cast<const lazy_server_config<Base>&>(cfg));
    case server_config_type::socket:
      return f(static_cast<const socket_server_config<Base>&>(cfg));
    default:
      assert(type == server_config_type::fail);
      return f(static_cast<const fail_server_config<Base>&>(cfg));
  }
}

/// Gets a pointer to a specific subtype of a server configuration.
template <class T, class Base>
T* get_if(server_config<Base>* cfg) {
  if (T::type_token == cfg->type())
    return static_cast<T*>(cfg);
  return nullptr;
}

/// Gets a pointer to a specific subtype of a server configuration.
template <class T, class Base>
const T* get_if(const server_config<Base>* cfg) {
  if (T::type_token == cfg->type())
    return static_cast<const T*>(cfg);
  return nullptr;
}

/// Creates a `fail_server_config` from another configuration object plus error.
template <class Base>
auto to_fail_config(server_config_ptr<Base> ptr, error err) {
  using impl_t = fail_server_config<Base>;
  return make_counted<impl_t>(std::move(err), ptr->mpx, ptr->trait);
}

/// Returns the pointer as a pointer to the `server_config` base type.
template <class T>
auto as_base_ptr(
  intrusive_ptr<T> ptr,
  std::enable_if_t<std::is_base_of_v<server_config_tag, T>>* = nullptr) {
  return std::move(ptr).template upcast<typename T::base_type>();
}

} // namespace caf::net::dsl
