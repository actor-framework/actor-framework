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

#include "caf/make_message.hpp"
#include "caf/message.hpp"
#include "caf/stream_sink.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

/// Dispatches a finalize call to a function taking either one or two arguments.
template <class Fun, class State,
          bool AcceptsTwoArgs =
            detail::is_callable_with<Fun, State&, const error&>::value>
struct stream_finalize_trait;

/// Specializes the trait for callbacks that only take the state.
template <class Fun, class State>
struct stream_finalize_trait<Fun, State, false>{
  static void invoke(Fun& f, State& st, const error&) {
    static_assert(detail::is_callable_with<Fun, State&>::value,
                  "Finalize function neither accepts (State&, const error&) "
                  "nor (State&)");
    f(st);
  }
};

/// Specializes the trait for callbacks that take state and error.
template <class Fun, class State>
struct stream_finalize_trait<Fun, State, true>{
  static void invoke(Fun& f, State& st, const error& err) {
    f(st, err);
  }
};


} // namespace caf

