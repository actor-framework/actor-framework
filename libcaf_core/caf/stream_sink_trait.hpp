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

#ifndef CAF_STREAM_SINK_TRAIT_HPP
#define CAF_STREAM_SINK_TRAIT_HPP

#include "caf/message.hpp"
#include "caf/make_message.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

namespace detail {

// -- invoke helper to support element-wise and batch-wise processing ----------

struct stream_sink_trait_invoke_one {
  template <class F, class State, class In>
  static void invoke(F& f, State& st, std::vector<In>&& xs) {
    for (auto& x : xs)
      f(st, std::move(x));
  }
};

struct stream_sink_trait_invoke_all {
  template <class F, class State, class In>
  static void invoke(F& f, State& st, std::vector<In>&& xs) {
    f(st, std::move(xs));
  }
};

// -- result helper to support void and non-void functions ---------------------

struct stream_sink_trait_default_finalize {
  template <class F, class State>
  static message invoke(F& f, State& st) {
    return make_message(f(st));
  }
};

struct stream_sink_trait_void_finalize {
  template <class F, class State>
  static message invoke(F& f, State& st) {
    f(st);
    return make_message();
  }
};

} // namespace detail

// -- trait implementation -----------------------------------------------------

/// Base type for all sink traits.
template <class State, class In, class Out>
struct stream_sink_trait_base {
  /// Defines the state element for the function objects.
  using state = State;

  /// Defines the type of a single stream element.
  using input = In;

  /// Defines the result type of the sink.
  using output = Out;
};

/// Defines required type aliases for stream sinks.
template <class Fun, class Fin>
struct stream_sink_trait;

/// Specializes the trait for element-wise processing with result.
template <class State, class In, class Out>
struct stream_sink_trait<void(State&, In), Out(State&)>
    : stream_sink_trait_base<State, In, Out> {
  /// Defines a helper for dispatching to the finalizing function object.
  using finalize = detail::stream_sink_trait_default_finalize;

  /// Defines a helper for dispatching to the processing function object.
  using process = detail::stream_sink_trait_invoke_one;
};

/// Specializes the trait for element-wise processing without result.
template <class State, class In>
struct stream_sink_trait<void(State&, In), void(State&)>
    : stream_sink_trait_base<State, In, void> {
  /// Defines a helper for dispatching to the finalizing function object.
  using finalize = detail::stream_sink_trait_void_finalize;

  /// Defines a helper for dispatching to the processing function object.
  using process = detail::stream_sink_trait_invoke_one;
};

/// Specializes the trait for batch-wise processing with result.
template <class State, class In, class Out>
struct stream_sink_trait<void(State&, std::vector<In>&), Out(State&)>
    : stream_sink_trait_base<State, In, Out> {
  /// Defines a helper for dispatching to the finalizing function object.
  using finalize = detail::stream_sink_trait_default_finalize;

  /// Defines a helper for dispatching to the processing function object.
  using process = detail::stream_sink_trait_invoke_all;
};

/// Specializes the trait for batch-wise processing without result.
template <class State, class In>
struct stream_sink_trait<void(State&, std::vector<In>&), void(State&)>
    : stream_sink_trait_base<State, In, void> {
  /// Defines a helper for dispatching to the finalizing function object.
  using finalize = detail::stream_sink_trait_void_finalize;

  /// Defines a helper for dispatching to the processing function object.
  using process = detail::stream_sink_trait_invoke_all;
};

// -- convenience alias --------------------------------------------------------

/// Derives a sink trait from the signatures of Fun and Fin.
template <class Fun, class Fin>
using stream_sink_trait_t =
  stream_sink_trait<typename detail::get_callable_trait<Fun>::fun_sig,
                    typename detail::get_callable_trait<Fin>::fun_sig>;

} // namespace caf

#endif // CAF_STREAM_SINK_TRAIT_HPP
