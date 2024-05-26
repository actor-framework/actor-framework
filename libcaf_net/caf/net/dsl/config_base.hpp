// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/checked_socket.hpp"
#include "caf/net/dsl/base.hpp"
#include "caf/net/dsl/get_name.hpp"
#include "caf/net/dsl/has_make_ctx.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/ssl/connection.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/stream_socket.hpp"

#include "caf/callback.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/error.hpp"
#include "caf/format_to_error.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/ref_counted.hpp"
#include "caf/sec.hpp"
#include "caf/uri.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <utility>

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

  /// Convenience function for setting a default error if `as_has_make_ctx`
  /// returns `nullptr` while trying to set an SSL context.
  error cannot_add_ctx() {
    return format_to_error(sec::logic_error,
                           "cannot add an SSL context or context factory "
                           "to a config of type {}",
                           name());
  }

  /// Inspects the data of this configuration and returns a pointer to it as
  /// `has_make_ctx` instance if possible, `nullptr` otherwise.
  virtual has_make_ctx* as_has_make_ctx() noexcept = 0;

  /// Inspects the data of this configuration and returns a pointer to it as
  /// `has_make_ctx` instance if possible, `nullptr` otherwise.
  virtual const has_make_ctx* as_has_make_ctx() const noexcept = 0;

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

template <class... Data>
class config_impl : public config_base {
public:
  using super = config_base;

  using variant_type = std::variant<error, Data...>;

  template <class... Args>
  explicit config_impl(multiplexer* mpx, Args&&... args)
    : super(mpx), data(std::forward<Args>(args)...) {
    // nop
  }

  template <class... Args>
  explicit config_impl(const config_base& from, Args&&... args)
    : super(from), data(std::forward<Args>(args)...) {
    // nop
  }

  variant_type data;

  template <class F>
  auto visit(F&& f) {
    return std::visit([&](auto& arg) { return f(arg); }, data);
  }

  /// Returns the name of the configuration type.
  std::string_view name() const noexcept override {
    auto f = [](const auto& val) noexcept {
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

  has_make_ctx* as_has_make_ctx() noexcept override {
    return has_make_ctx::from(data);
  }

  const has_make_ctx* as_has_make_ctx() const noexcept override {
    return has_make_ctx::from(data);
  }

  template <class From, class Token, class... Args>
  void assign(const From& from, Token, Args&&... args) {
    // Always construct the data in-place first. This makes sure we account
    // for ownership transfers (e.g. for sockets).
    data.template emplace<typename Token::type>(std::forward<Args>(args)...);
    // Discard the data if `from` contained an error. Otherwise, transfer the
    // SSL context over to the refined configuration.
    if (!from) {
      data.template emplace<error>(std::get<error>(from.data));
    } else if (auto* dst = this->as_has_make_ctx()) {
      if (const auto* src = from.as_has_make_ctx()) {
        dst->assign(src);
      } else {
        data = make_error(caf::sec::logic_error,
                          "failed to transfer SSL context");
      }
    }
  }

protected:
  template <class Derived, class From, class Token, class... Args>
  static intrusive_ptr<Derived> make_impl(std::in_place_type_t<Derived>,
                                          const From& from, Token token,
                                          Args&&... args) {
    // Always construct the data in-place first. This makes sure we account
    // for ownership transfers (e.g. for sockets).
    auto ptr = make_counted<Derived>(from, token, std::forward<Args>(args)...);
    // Discard the data if `from` contained an error. Otherwise, transfer the
    // SSL context over to the refined configuration.
    if (!from) {
      ptr->data.template emplace<error>(std::get<error>(from.data));
    } else if (auto* dst = ptr->as_has_make_ctx()) {
      if (const auto* src = from.as_has_make_ctx()) {
        dst->assign(src);
      } else {
        ptr->data = make_error(caf::sec::logic_error,
                               "failed to transfer SSL context");
      }
    }
    return ptr;
  }
};

} // namespace caf::net::dsl
