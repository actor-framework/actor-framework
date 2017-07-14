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

#ifndef CAF_COMPOSABLE_STATE_BASED_ACTOR_HPP
#define CAF_COMPOSABLE_STATE_BASED_ACTOR_HPP

#include "caf/stateful_actor.hpp"
#include "caf/message_handler.hpp"

namespace caf {

/// Implementation class for spawning composable states directly as actors.
template <class State, class Base = typename State::actor_base>
class composable_behavior_based_actor : public stateful_actor<State, Base> {
 public:
  static_assert(!std::is_abstract<State>::value,
                "State is abstract, please make sure to override all "
                "virtual operator() member functions");

  using super = stateful_actor<State, Base>;

  composable_behavior_based_actor(actor_config& cfg) : super(cfg) {
    // nop
  }

  using behavior_type = typename State::behavior_type;

  behavior_type make_behavior() override {
    this->state.init_selfptr(this);
    message_handler tmp;
    this->state.init_behavior(tmp);
    return behavior_type{typename behavior_type::unsafe_init{}, std::move(tmp)};
  }
};

} // namespace caf

#endif // CAF_COMPOSABLE_STATE_BASED_ACTOR_HPP
