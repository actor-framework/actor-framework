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

#include "caf/bound_actor.hpp"

#include "caf/mailbox_element.hpp"
#include "caf/system_messages.hpp"

#include "caf/detail/disposer.hpp"
#include "caf/detail/merged_tuple.hpp"

namespace caf {

bound_actor::bound_actor(actor_system* sys, actor_addr decorated, message msg)
    : abstract_actor(sys, decorated->id(), decorated->node(),
                     is_abstract_actor_flag | is_actor_bind_decorator_flag),
      decorated_(std::move(decorated)),
      merger_(std::move(msg)) {
  // nop
}

void bound_actor::attach(attachable_ptr ptr) {
  decorated_->attach(std::move(ptr));
}

size_t bound_actor::detach(const attachable::token& what) {
  return decorated_->detach(what);
}

void bound_actor::enqueue(mailbox_element_ptr what, execution_unit* host) {
  // ignore system messages
  auto& msg = what->msg;
  if (! ((msg.size() == 1 && (msg.match_element<exit_msg>(0)
                              || msg.match_element<down_msg>(0)))
         || (msg.size() > 1 && msg.match_element<sys_atom>(0)))) {
    // merge non-system messages
    message tmp{detail::merged_tuple::make(merger_, std::move(msg))};
    msg.swap(tmp);
  }
  decorated_->enqueue(std::move(what), host);
}

bool bound_actor::link_impl(linking_operation op, const actor_addr& other) {
  if (actor_cast<abstract_actor*>(other) == this || other == decorated_)
    return false;
  return decorated_->link_impl(op, other);
}

} // namespace caf
