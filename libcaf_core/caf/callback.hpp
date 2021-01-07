// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/error.hpp"

#include <memory>

namespace caf {

/// Describes a simple callback, usually implemented via lambda expression.
/// Callbacks are used as "type-safe function objects" wherever an interface
/// requires dynamic dispatching. The alternative would be to store the lambda
/// in a `std::function`, which adds another layer of indirection and always
/// requires a heap allocation. With the callback implementation of CAF, the
/// object may remains on the stack and do not cause more overhead than
/// necessary.
template <class Signature>
class callback;

template <class Result, class... Ts>
class callback<Result(Ts...)> {
public:
  virtual ~callback() {
    // nop
  }

  virtual Result operator()(Ts...) = 0;
};

/// Smart pointer type for heap-allocated callbacks with unique ownership.
template <class Signature>
using unique_callback_ptr = std::unique_ptr<callback<Signature>>;

/// Smart pointer type for heap-allocated callbacks with shared ownership.
template <class Signature>
using shared_callback_ptr = std::shared_ptr<callback<Signature>>;

/// Utility class for wrapping a function object of type `F`.
template <class F, class Signature>
class callback_impl;

template <class F, class Result, class... Ts>
class callback_impl<F, Result(Ts...)> final : public callback<Result(Ts...)> {
public:
  callback_impl(F&& f) : f_(std::move(f)) {
    // nop
  }

  callback_impl(callback_impl&&) = default;

  callback_impl& operator=(callback_impl&&) = default;

  Result operator()(Ts... xs) override {
    return f_(std::forward<Ts>(xs)...);
  }

private:
  F f_;
};

/// Wraps `fun` into a @ref callback function object.
/// @relates callback
template <class F>
auto make_callback(F fun) {
  using signature = typename detail::get_callable_trait<F>::fun_sig;
  return callback_impl<F, signature>{std::move(fun)};
}

/// Creates a heap-allocated, type-erased @ref callback from the function object
/// `fun`.
/// @relates callback
template <class F>
auto make_type_erased_callback(F fun) {
  using signature = typename detail::get_callable_trait<F>::fun_sig;
  using result_t = unique_callback_ptr<signature>;
  return result_t{new callback_impl<F, signature>{std::move(fun)}};
}

/// Creates a heap-allocated, type-erased @ref callback from the function object
/// `fun` with shared ownership.
/// @relates callback
template <class F>
auto make_shared_type_erased_callback(F fun) {
  using signature = typename detail::get_callable_trait<F>::fun_sig;
  auto res = std::make_shared<callback_impl<F, signature>>(std::move(fun));
  return shared_callback_ptr<signature>{std::move(res)};
}

} // namespace caf
