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

#include <vector>

#include "caf/fwd.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

namespace detail {

// -- invoke helper to support element-wise and batch-wise processing ----------

struct stream_stage_trait_invoke_one {
  template <class F, class State, class Out, class In>
  static void invoke(F& f, State& st, downstream<Out>& out,
                     std::vector<In>& xs) {
    for (auto& x : xs)
      f(st, out, std::move(x));
  }
};

struct stream_stage_trait_invoke_all {
  template <class F, class State, class Out, class In>
  static void invoke(F& f, State& st, downstream<Out>& out,
                     std::vector<In>& xs) {
    f(st, out, xs);
  }
};

} // namespace detail

// -- trait implementation -----------------------------------------------------

template <class F>
struct stream_stage_trait {
  static constexpr bool valid = false;
  using output = unit_t;
};

/// Deduces the input type, output type and the state type for a stream stage
/// from its `process` implementation.
template <class State, class In, class Out>
struct stream_stage_trait<void (State&, downstream<Out>&, In)> {
  static constexpr bool valid = true;
  using state = State;
  using input = In;
  using output = Out;
  using process = detail::stream_stage_trait_invoke_one;
};

template <class State, class In, class Out>
struct stream_stage_trait<void (State&, downstream<Out>&, std::vector<In>&)> {
  static constexpr bool valid = true;
  using state = State;
  using input = In;
  using output = Out;
  using process = detail::stream_stage_trait_invoke_all;
};

// -- convenience alias --------------------------------------------------------

/// Convenience alias for extracting the function signature from `Process` and
/// passing it to `stream_stage_trait`.
template <class Process>
using stream_stage_trait_t =
  stream_stage_trait<typename detail::get_callable_trait<Process>::fun_sig>;

} // namespace caf

