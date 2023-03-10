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
  using trait_type = typename Base::trait_type;

  /// Sets the optional SSL context.
  ///
  /// @param ctx The SSL context for encryption.
  /// @returns a reference to `*this`.
  Subtype& context(expected<ssl::context> ctx) {
    auto& dref = static_cast<Subtype&>(*this);
    dref.get_context() = std::move(ctx);
    return dref;
  }
};

} // namespace caf::net::dsl
