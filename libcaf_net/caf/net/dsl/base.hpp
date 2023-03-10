// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/fwd.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/ssl/fwd.hpp"

namespace caf::net::dsl {

/// Base type for our DSL classes to configure a factory object..
template <class Trait>
class base {
public:
  using trait_type = Trait;

  virtual ~base() {
    // nop
  }

  /// @returns the pointer to the @ref multiplexer.
  virtual multiplexer* mpx() const noexcept = 0;

  /// @returns the trait object.
  virtual const Trait& trait() const noexcept = 0;

  /// @returns the optional SSL context, whereas an object with
  ///          default-constructed error is treated as "no SSL".
  expected<ssl::context>& get_context() {
    return get_context_impl();
  }

  /// @private
  template <class ConfigType>
  auto with_context(intrusive_ptr<ConfigType> ptr) {
    using ConfigBaseType = typename ConfigType::super;
    auto as_base_ptr = [](auto& derived_ptr) {
      return std::move(derived_ptr).template upcast<ConfigBaseType>();
    };
    // Move the context into the config if present.
    auto& ctx = get_context();
    if (ctx) {
      ptr->ctx = std::make_shared<ssl::context>(std::move(*ctx));
      return as_base_ptr(ptr);
    }
    // Default-constructed error just means "no SSL".
    if (!ctx.error())
      return as_base_ptr(ptr);
    // We actually have an error: replace `ptr` with a fail config. Need to cast
    // to the base type for to_fail_config to pick up the right overload.
    auto fptr = to_fail_config(as_base_ptr(ptr), std::move(ctx.error()));
    return as_base_ptr(fptr);
  }

private:
  virtual expected<ssl::context>& get_context_impl() noexcept = 0;
};

} // namespace caf::net::dsl
