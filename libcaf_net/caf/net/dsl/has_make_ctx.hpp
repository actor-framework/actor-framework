// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/ssl/connection.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/ssl/tcp_acceptor.hpp"
#include "caf/net/stream_socket.hpp"

#include "caf/expected.hpp"

#include <memory>
#include <variant>

namespace caf::net::dsl {

/// Configuration for an endpoint that stores a SSL context factory for secure
/// networking.
class has_make_ctx {
public:
  using ctx_ptr = std::shared_ptr<ssl::context>;

  using ctx_factory = std::function<expected<ctx_ptr>()>;

  template <class SumType>
  static auto from(SumType& data) noexcept {
    using result_t = std::conditional_t<std::is_const_v<SumType>,
                                        const has_make_ctx*, has_make_ctx*>;
    auto get_ptr = [](auto& val) -> result_t {
      using val_t = std::decay_t<decltype(val)>;
      if constexpr (std::is_base_of_v<has_make_ctx, val_t>)
        return &val;
      else
        return nullptr;
    };
    return std::visit(get_ptr, data);
  }

  void assign(const has_make_ctx* other) noexcept {
    make_ctx = other->make_ctx;
  }

  /// SSL context factory for lazy loading SSL on demand.
  ctx_factory make_ctx;
};

} // namespace caf::net::dsl
