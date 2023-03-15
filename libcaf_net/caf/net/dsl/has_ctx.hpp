// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/expected.hpp"
#include "caf/net/ssl/acceptor.hpp"
#include "caf/net/ssl/connection.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/stream_socket.hpp"

#include <memory>
#include <variant>

namespace caf::net::dsl {

/// Configuration for a client that uses a user-provided socket.
class has_ctx {
public:
  /// SSL context for secure servers.
  std::shared_ptr<ssl::context> ctx;

  /// Returns a function that, when called with a @ref stream_socket, calls
  /// `f` either with a new SSL connection from `ctx` or with the file the
  /// file descriptor if no SSL context is defined.
  template <class F>
  auto connection_with_ctx(F&& f) {
    return [this, g = std::forward<F>(f)](stream_socket fd) mutable {
      using res_t = decltype(g(fd));
      if (ctx) {
        auto conn = ctx->new_connection(fd);
        if (conn)
          return g(*conn);
        close(fd);
        return res_t{std::move(conn.error())};
      } else
        return g(fd);
    };
  }

  /// Returns a function that, when called with an accept socket, calls `f`
  /// either with a new SSL acceptor from `ctx` or with the file the file
  /// descriptor if no SSL context is defined.
  template <class F>
  auto acceptor_with_ctx(F&& f) {
    return [this, g = std::forward<F>(f)](auto fd) mutable {
      if (ctx) {
        auto acc = ssl::acceptor{fd, std::move(*ctx)};
        return g(acc);
      } else
        return g(fd);
    };
  }

  template <class SumType>
  static has_ctx* from(SumType& data) noexcept {
    auto get_ptr = [](auto& val) -> has_ctx* {
      using val_t = std::decay_t<decltype(val)>;
      if constexpr (std::is_base_of_v<has_ctx, val_t>)
        return &val;
      else
        return nullptr;
    };
    return std::visit(get_ptr, data);
  }
};

} // namespace caf::net::dsl
