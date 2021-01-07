// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

namespace caf::detail {

template <class... Fs>
struct overload;

template <class F>
struct overload<F> : F {
  using F::operator();
  overload(F f) : F(f) {
    // nop
  }
};

template <class F, class... Fs>
struct overload<F, Fs...> : F, overload<Fs...> {
  using F::operator();
  using overload<Fs...>::operator();
  overload(F f, Fs... fs) : F(f), overload<Fs...>(fs...) {
    // nop
  }
};

template <class... Fs>
overload<Fs...> make_overload(Fs... fs) {
  return {fs...};
}

} // namespace caf::detail
