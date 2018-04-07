/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
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

namespace caf {
namespace detail {

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

} // namespace detail
} // namespace caf

