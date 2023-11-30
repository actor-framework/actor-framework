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
  using ctx_factory = std::function<expected<ssl::context>()>;

  has_make_ctx() = default;

  has_make_ctx(has_make_ctx&&) = default;

  has_make_ctx(const has_make_ctx&) = default;

  has_make_ctx& operator=(has_make_ctx&&) = default;

  has_make_ctx& operator=(const has_make_ctx&) = default;

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

  template <typename F>
  void make_ctx(F&& factory) noexcept {
    make_ctx_ = std::forward<F>(factory);
  }

  auto& make_ctx() noexcept {
    return make_ctx_;
  }

  bool make_ctx_valid() const noexcept {
    return static_cast<bool>(make_ctx_);
  }

  void assign(const has_make_ctx* other) noexcept {
    make_ctx_ = other->make_ctx_;
  }

private:
  /// SSL context factory for lazy loading SSL on demand.
  ctx_factory make_ctx_;
};

} // namespace caf::net::dsl
