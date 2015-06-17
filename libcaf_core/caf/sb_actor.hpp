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

#ifndef CAF_FSM_ACTOR_HPP
#define CAF_FSM_ACTOR_HPP

#include <utility>
#include <type_traits>

#include "caf/config.hpp"
#include "caf/event_based_actor.hpp"

namespace caf {

// <backward_compatibility version="0.13">
template <class Derived, class Base = event_based_actor>
class sb_actor : public Base {
public:
  static_assert(std::is_base_of<event_based_actor, Base>::value,
                "Base must be event_based_actor or a derived type");

  behavior make_behavior() override {
    return static_cast<Derived*>(this)->init_state;
  }

protected:
  using combined_type = sb_actor;

  template <class... Ts>
  sb_actor(Ts&&... xs) : Base(std::forward<Ts>(xs)...) {
    // nop
  }
} CAF_DEPRECATED ;
// </backward_compatibility>

} // namespace caf

#endif // CAF_FSM_ACTOR_HPP
