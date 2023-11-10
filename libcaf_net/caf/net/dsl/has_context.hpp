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
    if (auto* ptr = cfg.as_has_make_ctx())
      ptr->context_factory(
        [ctx = std::make_shared<ssl::context>(std::move(
           ctx))]() -> expected<std::shared_ptr<ssl::context>> { return ctx; });
    else if (cfg)
      cfg.fail(cfg.cannot_add_ctx());
    return dref;
  }

  /// Sets the optional SSL context.
  /// @param ctx The SSL context for encryption.
  /// @returns a reference to `*this`.
  Subtype& context(expected<ssl::context> ctx) {
    auto& dref = static_cast<Subtype&>(*this);
    // If we pass a default constructed error inside the expected type, we don't
    // want to fail, but use the default SSL factory.
    if (ctx)
      context(std::move(*ctx));
    else if (ctx.error())
      dref.config().fail(std::move(ctx).error());
    return dref;
  }
};

} // namespace caf::net::dsl
