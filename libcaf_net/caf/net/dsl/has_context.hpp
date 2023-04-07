// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/expected.hpp"
#include "caf/net/ssl/context.hpp"

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
    if (auto* ptr = cfg.as_has_ctx())
      ptr->ctx = std::make_shared<ssl::context>(std::move(ctx));
    else if (cfg)
      cfg.fail(cfg.cannot_add_ctx());
    return dref;
  }

  /// Sets the optional SSL context.
  /// @param ctx The SSL context for encryption.
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
