// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/callback.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/error.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/net/checked_socket.hpp"
#include "caf/net/dsl/base.hpp"
#include "caf/net/dsl/get_name.hpp"
#include "caf/net/dsl/has_ctx.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/ssl/connection.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/ref_counted.hpp"
#include "caf/sec.hpp"
#include "caf/uri.hpp"

#include <cassert>
#include <cstdint>
#include <string>

namespace caf::net::dsl {

/// Base class for configuration objects.
class CAF_NET_EXPORT config_base : public ref_counted {
public:
  explicit config_base(multiplexer* mpx) : mpx(mpx) {
    // nop
  }

  config_base(const config_base&) = default;

  config_base& operator=(const config_base&) = default;

  virtual ~config_base();

  virtual std::string_view name() const noexcept = 0;

  virtual void fail(error err) noexcept = 0;

  virtual error fail_reason() const = 0;

  /// Convenience function for setting a default error if `as_has_ctx` returns
  /// `nullptr` while trying to set an SSL context.
  error cannot_add_ctx() {
    return make_error(sec::logic_error,
                      "cannot add an SSL context to a config of type "
                        + std::string{name()});
  }

  virtual has_ctx* as_has_ctx() noexcept = 0;

  bool failed() const noexcept {
    return name() == get_name<error>::value;
  }

  explicit operator bool() const noexcept {
    return !failed();
  }

  bool operator!() const noexcept {
    return failed();
  }

  /// The pointer to the parent @ref multiplexer.
  multiplexer* mpx;

  /// User-defined callback for errors.
  shared_callback_ptr<void(const error&)> on_error;

  /// Calls `on_error` if non-null.
  void call_on_error(const error& what) {
    if (on_error)
      (*on_error)(what);
  }
};

/// Simple base class for a configuration with a trait member.
template <class Trait>
class config_with_trait : public config_base {
public:
  using trait_type = Trait;

  explicit config_with_trait(multiplexer* mpx) : config_base(mpx) {
    // nop
  }

  config_with_trait(const config_with_trait&) = default;

  config_with_trait& operator=(const config_with_trait&) = default;

  /// Configures various aspects of the protocol stack such as in- and output
  /// types.
  Trait trait;
};

template <class Base, class... Data>
class config_impl : public Base {
public:
  using super = Base;

  static_assert(std::is_base_of_v<config_base, super>);

  template <class From, class... Args>
  explicit config_impl(From&& from, Args&&... args)
    : super(std::forward<From>(from)), data(std::forward<Args>(args)...) {
    if constexpr (std::is_base_of_v<config_base, std::decay_t<From>>) {
      if (!from)
        data = from.fail_reason();
    }
  }

  std::variant<error, Data...> data;

  template <class F>
  auto visit(F&& f) {
    return std::visit([&](auto& arg) { return f(arg); }, data);
  }

  /// Returns the name of the configuration type.
  std::string_view name() const noexcept override {
    auto f = [](const auto& val) {
      using val_t = std::decay_t<decltype(val)>;
      return get_name<val_t>::value;
    };
    return std::visit(f, data);
  }

  void fail(error err) noexcept override {
    if (!std::holds_alternative<error>(data))
      data = std::move(err);
  }

  error fail_reason() const override {
    if (auto* val = std::get_if<error>(&data))
      return *val;
    else
      return {};
  }

  has_ctx* as_has_ctx() noexcept override {
    return has_ctx::from(data);
  }
};

} // namespace caf::net::dsl
