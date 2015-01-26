/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#include "caf/message_id.hpp"
#include "caf/event_based_actor.hpp"

#include "caf/detail/singletons.hpp"

namespace caf {

event_based_actor::~event_based_actor() {
  // nop
}

void event_based_actor::forward_to(const actor& whom,
                                   message_priority prio) {
  forward_message(whom, prio);
}

behavior event_based_actor::functor_based::make_behavior() {
  auto res = m_make_behavior(this);
  // reset make_behavior_fun to get rid of any
  // references held by the function object
  make_behavior_fun tmp;
  m_make_behavior.swap(tmp);
  return res;
}

} // namespace caf
