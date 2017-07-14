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

#ifndef CAF_STREAM_SINK_TRAIT_HPP
#define CAF_STREAM_SINK_TRAIT_HPP

#include "caf/message.hpp"
#include "caf/make_message.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

template <class Fun, class Fin>
struct stream_sink_trait;

template <class State, class In, class Out>
struct stream_sink_trait<void (State&, In), Out (State&)> {
  using state = State;
  using input = In;
  using output = Out;
  template <class F>
  static message make_result(state& st, F f) {
    return make_message(f(st));
  }
};

template <class State, class In>
struct stream_sink_trait<void (State&, In), void (State&)> {
  using state = State;
  using input = In;
  using output = void;
  template <class F>
  static message make_result(state& st, F& f) {
    f(st);
    return make_message();
  }
};

template <class Fun, class Fin>
using stream_sink_trait_t =
  stream_sink_trait<typename detail::get_callable_trait<Fun>::fun_sig,
                    typename detail::get_callable_trait<Fin>::fun_sig>;

} // namespace caf

#endif // CAF_STREAM_SINK_TRAIT_HPP
