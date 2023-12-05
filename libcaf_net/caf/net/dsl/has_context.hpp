// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/ssl/context.hpp"

#include "caf/expected.hpp"

namespace caf::net::dsl {

/// DSL entry point for creating a server.
template <class Base, class Subtype>
class has_context : public Base {
public:
  /// Sets the optional SSL context.
  /// @param ctx The SSL context for encryption.
  /// @returns a reference to `*this`.
  Subtype& context(ssl::context ctx) {
    auto& dref = static_cast<Subtype&>(*this);
    auto& cfg = dref.config();
    if (auto* ptr = cfg.as_has_make_ctx()) {
      auto ctx_ptr = std::make_shared<ssl::context>(std::move(ctx));
      ptr->make_ctx = [ctx_ptr]() -> expected<std::shared_ptr<ssl::context>> {
        return ctx_ptr;
      };
    } else if (cfg) {
      cfg.fail(cfg.cannot_add_ctx());
    }
    return dref;
  }

  /// Sets the optional SSL context.
  /// @param ctx The SSL context for encryption. Passing an `expected` with a
  ///            default-constructed `error` results in a no-op.
  /// @returns a reference to `*this`.
  Subtype& context(expected<ssl::context> ctx) {
    auto& dref = static_cast<Subtype&>(*this);
    if (ctx)
      context(std::move(*ctx));
    else if (ctx.error())
      dref.config().fail(std::move(ctx).error());
    return dref;
  }
};

} // namespace caf::net::dsl
