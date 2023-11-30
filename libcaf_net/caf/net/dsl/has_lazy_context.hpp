// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/ssl/context.hpp"

#include "caf/expected.hpp"

namespace caf::net::dsl {

/// DSL entry point for postponing the creation of a ssl context, if and when
/// needed by the protocol.
template <class Base, class Subtype>
class has_lazy_context : public Base {
public:
  /// Sets the optional SSL context factory.
  /// @param factory The function creating the SSL context for encryption.
  /// @returns a reference to `*this`.
  template <typename F>
  Subtype& context_factory(F&& factory) {
    auto& dref = static_cast<Subtype&>(*this);
    auto& cfg = dref.config();
    if (auto* ptr = cfg.as_has_make_ctx())
      ptr->make_ctx(std::forward<F>(factory));
    else if (cfg)
      cfg.fail(cfg.cannot_add_ctx());
    return dref;
  }
};

} // namespace caf::net::dsl
