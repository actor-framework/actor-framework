/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#ifndef CAF_EXPERIMENTAL_STATEFUL_ACTOR_HPP
#define CAF_EXPERIMENTAL_STATEFUL_ACTOR_HPP

#include <new>
#include <type_traits>

#include "caf/fwd.hpp"

namespace caf {
namespace experimental {

template <class State, class Base = event_based_actor>
class stateful_actor : public Base {
public:
  stateful_actor() {
    // nop
  }

  ~stateful_actor() {
    // nop
  }

  void initialize() override {
    cr_state(this);
    Base::initialize();
  }

  void on_exit() override final {
    state_.~State();
  }

  State& state() {
    return state_;
  }

private:
  template <class T>
  typename std::enable_if<std::is_constructible<State, T>::value>::type
  cr_state(T arg) {
    new (&state_) State(arg);
  }

  template <class T>
  typename std::enable_if<! std::is_constructible<State, T>::value>::type
  cr_state(T) {
    new (&state_) State();
  }

  union { State state_; };
};

} // namespace experimental
} // namespace caf

#endif // CAF_EXPERIMENTAL_STATEFUL_ACTOR_HPP
