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

#include "caf/decorator/sequencer.hpp"

#include "caf/actor_system.hpp"
#include "caf/default_attachable.hpp"

#include "caf/detail/disposer.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

namespace caf {
namespace decorator {

sequencer::sequencer(actor_addr f, actor_addr g, message_types_set msg_types)
    : monitorable_actor(&g->home_system(),
                        g->home_system().next_actor_id(),
                        g->node(),
                        is_abstract_actor_flag | is_actor_dot_decorator_flag),
      f_(std::move(f)),
      g_(std::move(g)),
      msg_types_(std::move(msg_types)) {
  // composed actor has dependency on constituent actors by default;
  // if either constituent actor is already dead upon establishing
  // the dependency, the actor is spawned dead
  f_->attach(default_attachable::make_monitor(address()));
  if (g_ != f_)
    g_->attach(default_attachable::make_monitor(address()));
}

void sequencer::enqueue(mailbox_element_ptr what,
                             execution_unit* context) {
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
  auto down_msg_handler = [&](const down_msg& dm) {
    // quit if either `f` or `g` are no longer available
    if (dm.source == f_ || dm.source == g_)
      monitorable_actor::cleanup(dm.reason, context);
  };
  // handle and consume the system message;
  // the only effect that MAY result from handling a system message
  // is to exit the actor if it hasn't exited already;
  // `handle_system_message()` is thread-safe, and if the actor
  // has already exited upon the invocation, nothing is done
  if (handle_system_message(*what, context, false, down_msg_handler))
    return;
  // process and forward the non-system message;
  // store `f` as the next stage in the forwarding chain
  what->stages.push_back(f_);
  // forward modified message to `g`
  g_->enqueue(std::move(what), context);
}

sequencer::message_types_set sequencer::message_types() const {
  return msg_types_;
}

} // namespace decorator
} // namespace caf
