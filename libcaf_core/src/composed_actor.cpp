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

#include "caf/composed_actor.hpp"

namespace caf {

composed_actor::composed_actor(actor_system* sys,
                               actor_addr first, actor_addr second)
    : local_actor(sys, first->id(), first->node(),
                  is_abstract_actor_flag | is_actor_dot_decorator_flag),
      first_(first),
      second_(second) {
  // nop
}

void composed_actor::initialize() {
  trap_exit(true);
  link_to(first_);
  link_to(second_);
  do_become(
    {
      [=](const exit_msg& em) {
        if (em.source == first_ || em.source == second_)
          quit(em.reason);
        else if (em.reason != exit_reason::normal)
          quit(em.reason);
      },
      others >> [] {
        // drop
      }
    },
    true
  );
}

void composed_actor::enqueue(mailbox_element_ptr what, execution_unit* host) {
  if (! what)
    return;
  if (is_system_message(what->msg)) {
    local_actor::enqueue(std::move(what), host);
    return;
  }
  // store second_ as the next stage in the forwarding chain
  what->stages.push_back(second_);
  // forward modified message to first_
  first_->enqueue(std::move(what), host);
}

bool composed_actor::is_system_message(const message& msg) {
  return (msg.size() == 1 && (msg.match_element<exit_msg>(0)
                              || msg.match_element<down_msg>(0)))
         || (msg.size() > 1 && msg.match_element<sys_atom>(0));
}

} // namespace caf
