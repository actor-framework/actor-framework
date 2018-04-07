/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include "caf/error.hpp"
#include "caf/config.hpp"

#include "caf/detail/type_traits.hpp"

// The class `callback` intentionally has no virtual destructor, because
// the lifetime of callback objects is never managed via base pointers.
CAF_PUSH_NON_VIRTUAL_DTOR_WARNING

namespace caf {

/// Describes a simple callback, usually implemented via lambda expression.
/// Callbacks are used as "type-safe function objects" wherever an interface
/// requires dynamic dispatching. The alternative would be to store the lambda
/// in a `std::function`, which adds another layer of indirection and
/// requires a heap allocation. With the callback implementation of CAF,
/// the object remains on the stack and does not cause more overhead
/// than necessary.
template <class... Ts>
class callback {
public:
  virtual error operator()(Ts...) = 0;
};

/// Utility class for wrapping a function object of type `Base`.
template <class F, class... Ts>
class callback_impl : public callback<Ts...> {
public:
  callback_impl(F&& f) : f_(std::move(f)) {
    // nop
  }

  callback_impl(callback_impl&&) = default;
  callback_impl& operator=(callback_impl&&) = default;

  error operator()(Ts... xs) override {
    return f_(xs...);
  }

private:
  F f_;
};

/// Utility class for selecting a `callback_impl`.
template <class F,
          class Args = typename detail::get_callable_trait<F>::arg_types>
struct select_callback;

template <class F, class... Ts>
struct select_callback<F, detail::type_list<Ts...>> {
  using type = callback_impl<F, Ts...>;
};

/// Creates a callback from a lambda expression.
/// @relates callback
template <class F>
typename select_callback<F>::type make_callback(F fun) {
  return {std::move(fun)};
}

} // namespace caf

CAF_POP_WARNINGS

