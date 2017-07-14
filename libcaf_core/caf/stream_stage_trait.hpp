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

#ifndef CAF_STREAM_STAGE_TRAIT_HPP
#define CAF_STREAM_STAGE_TRAIT_HPP

#include "caf/fwd.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {

template <class F>
struct stream_stage_trait;

template <class State, class In, class Out>
struct stream_stage_trait<void (State&, downstream<Out>&, In)> {
  using state = State;
  using input = In;
  using output = Out;
};

template <class F>
using stream_stage_trait_t =
  stream_stage_trait<typename detail::get_callable_trait<F>::fun_sig>;

} // namespace caf

#endif // CAF_STREAM_STAGE_TRAIT_HPP
