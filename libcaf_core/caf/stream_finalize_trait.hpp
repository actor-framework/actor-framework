// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/type_traits.hpp"
#include "caf/message.hpp"
#include "caf/stream_sink.hpp"

namespace caf {

/// Dispatches a finalize call to a function taking either one or two arguments.
template <class Fun, class State,
          bool AcceptsTwoArgs
          = detail::is_callable_with<Fun, State&, const error&>::value>
struct stream_finalize_trait;

/// Specializes the trait for callbacks that only take the state.
template <class Fun, class State>
struct stream_finalize_trait<Fun, State, false> {
  static void invoke(Fun& f, State& st, const error&) {
    static_assert(detail::is_callable_with<Fun, State&>::value,
                  "Finalize function neither accepts (State&, const error&) "
                  "nor (State&)");
    f(st);
  }
};

/// Specializes the trait for callbacks that take state and error.
template <class Fun, class State>
struct stream_finalize_trait<Fun, State, true> {
  static void invoke(Fun& f, State& st, const error& err) {
    f(st, err);
  }
};

} // namespace caf
