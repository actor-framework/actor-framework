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

#ifndef CAF_STREAM_STAGE_TRAIT_HPP
#define CAF_STREAM_STAGE_TRAIT_HPP

#include <vector>

#include "caf/fwd.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

namespace detail {

// -- invoke helper to support element-wise and batch-wise processing ----------

struct stream_stage_trait_invoke_one {
  template <class F, class State, class In, class Out>
  static void invoke(F& f, State& st, std::vector<In>&& xs,
                     downstream<Out>& out) {
    for (auto& x : xs)
      f(st, out, std::move(x));
  }
};

struct stream_stage_trait_invoke_all {
  template <class F, class State, class In, class Out>
  static void invoke(F& f, State& st, std::vector<In>&& xs,
                     downstream<Out>& out) {
    f(st, std::move(xs), out);
  }
};

} // namespace detail

// -- trait implementation -----------------------------------------------------

template <class F>
struct stream_stage_trait;

template <class State, class In, class Out>
struct stream_stage_trait<void (State&, downstream<Out>&, In)> {
  using state = State;
  using input = In;
  using output = Out;
  using invoke = detail::stream_stage_trait_invoke_one;
};

template <class State, class In, class Out>
struct stream_stage_trait<void (State&, std::vector<In>&&, downstream<Out>&)> {
  using state = State;
  using input = In;
  using output = Out;
  using invoke = detail::stream_stage_trait_invoke_all;
};

// -- convenience alias --------------------------------------------------------

template <class F>
using stream_stage_trait_t =
  stream_stage_trait<typename detail::get_callable_trait<F>::fun_sig>;

} // namespace caf

#endif // CAF_STREAM_STAGE_TRAIT_HPP
