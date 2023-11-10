// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/ssl/connection.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/ssl/tcp_acceptor.hpp"
#include "caf/net/stream_socket.hpp"

#include "caf/expected.hpp"

#include <memory>
#include <variant>

namespace caf::net::dsl {

/// Configuration for an endpoint that stores a SSL context for secure
/// networking.
class has_make_ctx {
public:
  using ctx_ptr = std::shared_ptr<ssl::context>;

  using ctx_factory = std::function<expected<ctx_ptr>()>;

  has_make_ctx() = default;

  has_make_ctx(has_make_ctx&&) = default;

  has_make_ctx(const has_make_ctx&) = default;

  has_make_ctx& operator=(has_make_ctx&&) = default;

  has_make_ctx& operator=(const has_make_ctx&) = default;

  template <class SumType>
  static has_make_ctx* from(SumType& data) noexcept {
    auto get_ptr = [](auto& val) -> has_make_ctx* {
      using val_t = std::decay_t<decltype(val)>;
      if constexpr (std::is_base_of_v<has_make_ctx, val_t>)
        return &val;
      else
        return nullptr;
    };
    return std::visit(get_ptr, data);
  }

  template <class SumType>
  static const has_make_ctx* from(const SumType& data) noexcept {
    auto get_ptr = [](const auto& val) -> const has_make_ctx* {
      using val_t = std::decay_t<decltype(val)>;
      if constexpr (std::is_base_of_v<has_make_ctx, val_t>)
        return &val;
      else
        return nullptr;
    };
    return std::visit(get_ptr, data);
  }

  template <typename F>
  void context_factory(F&& factory) noexcept {
    using invoke_res_t = std::decay_t<std::invoke_result_t<F>>;
    if constexpr (std::is_same_v<invoke_res_t, expected<ssl::context>>)
      context_factory_
        = [ctx = expected<ctx_ptr>{nullptr},
           factory = std::forward<F>(factory)]() mutable -> expected<ctx_ptr> {
        if (!ctx.has_value() || ctx == nullptr) {
          ctx = factory().and_then([](auto c) -> expected<ctx_ptr> {
            return std::make_shared<ssl::context>(std::move(c));
          });
        }
        return ctx;
      };
    else
      context_factory_ = std::forward<F>(factory);
  }

  auto make_ctx() noexcept {
    return context_factory_();
  }

  void assign(const has_make_ctx* other) noexcept {
    context_factory_ = other->context_factory_;
  }

private:
  // Default factory function. If not manually set up, don't use a SSL at all.
  static expected<std::shared_ptr<ssl::context>> default_ssl_factory() {
    return nullptr;
  };

  /// SSL context factory for lazy loading SSL on demand.
  ctx_factory context_factory_ = default_ssl_factory;
};

} // namespace caf::net::dsl
