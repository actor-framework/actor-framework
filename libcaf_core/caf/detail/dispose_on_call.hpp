// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/type_traits.hpp"
#include "caf/disposable.hpp"

#include <iostream>

namespace caf::detail {

template <class Signature>
struct dispose_on_call_t;

template <class R, class... Ts>
struct dispose_on_call_t<R(Ts...)> {
  template <class F>
  auto operator()(disposable resource, F f) {
    return [resource{std::move(resource)}, f{std::move(f)}](Ts... xs) mutable {
      std::cout << "Disposing " << resource.ptr() << std::endl;
      resource.dispose();
      return f(xs...);
    };
  }
};

/// Returns a decorator for the function object `f` that calls
/// `resource.dispose()` before invoking `f`.
template <class F>
auto dispose_on_call(disposable resource, F f) {
  using sig = typename get_callable_trait_t<F>::fun_sig;
  dispose_on_call_t<sig> factory;
  return factory(std::move(resource), std::move(f));
}

} // namespace caf::detail
