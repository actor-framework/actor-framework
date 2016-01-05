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

#include "caf/actor_cast.hpp"
#include "caf/actor_system.hpp"
#include "caf/default_attachable.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/system_messages.hpp"

#include "caf/detail/disposer.hpp"
#include "caf/detail/merged_tuple.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

namespace caf {

bound_actor::bound_actor(actor_addr decorated, message msg)
    : monitorable_actor{ &decorated->home_system(),
                         decorated->home_system().next_actor_id(),
                         decorated->node(),
                         is_abstract_actor_flag
                           | is_actor_bind_decorator_flag },
      decorated_{ std::move(decorated) },
      merger_{ std::move(msg) } {
  // bound actor has dependency on the decorated actor by default;
  // if the decorated actor is already dead upon establishing the
  // dependency, the actor is spawned dead
  decorated_->attach(default_attachable::make_monitor(address()));
}

void bound_actor::enqueue(mailbox_element_ptr what,
                          execution_unit* host) {
  if (! what)
    return; // not even an empty message
  auto reason = exit_reason_.load();
  if (reason != exit_reason::not_exited) {
    // actor has exited
    auto& mid = what->mid;
    if (mid.is_request()) {
      // make sure that a request always gets a response;
      // the exit reason reflects the first actor on the
      // forwarding chain that is out of service
      detail::sync_request_bouncer rb{reason};
      rb(what->sender, mid);
    }
    return;
  }
  if (is_system_message(what->msg)) {
    // handle and consume the system message;
    // the only effect that MAY result from handling a system message
    // is to exit the actor if it hasn't exited already;
    // `handle_system_message()` is thread-safe, and if the actor
    // has already exited upon the invocation, nothing is done
    handle_system_message(what->msg, host);
  } else {
    // process and forward the non-system message
    auto& msg = what->msg;
    message tmp{ detail::merged_tuple::make(merger_, std::move(msg)) };
    msg.swap(tmp);
    decorated_->enqueue(std::move(what), host);
  }
}

void bound_actor::handle_system_message(const message& msg,
                                        execution_unit* host) {
  // `monitorable_actor::cleanup()` is thread-safe, and if the actor
  // has already exited upon the invocation, nothing is done;
  // handles only `down_msg` from the decorated actor and `exit_msg` from anyone
  if (msg.size() != 1)
    return; // neither case
  if (msg.match_element<down_msg>(0)) {
    auto& dm = msg.get_as<down_msg>(0);
    CAF_ASSERT(dm.reason != exit_reason::not_exited);
    if (dm.source == decorated_) {
      // decorated actor has exited, so exits
      // the bound actor with the same reason
      monitorable_actor::cleanup(dm.reason, host);
    }
    return;
  }
  if (msg.match_element<exit_msg>(0)) {
    auto& em = msg.get_as<exit_msg>(0);
    CAF_ASSERT(em.reason != exit_reason::not_exited);
    // exit message received, so exits with the same reason
    monitorable_actor::cleanup(em.reason, host);
    return;
  }
  // drop other cases
}

bool bound_actor::is_system_message(const message& msg) {
  return (msg.size() == 1 && (msg.match_element<exit_msg>(0)
                             || msg.match_element<down_msg>(0)))
         || (msg.size() > 1 && msg.match_element<sys_atom>(0));
}

} // namespace caf
