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

#include "caf/decorator/sequencer.hpp"

#include "caf/actor_system.hpp"
#include "caf/default_attachable.hpp"

#include "caf/detail/disposer.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

namespace caf {
namespace decorator {

sequencer::sequencer(strong_actor_ptr f, strong_actor_ptr g,
                     message_types_set msg_types)
    : monitorable_actor(actor_config{}.add_flag(is_actor_dot_decorator_flag)),
      f_(std::move(f)),
      g_(std::move(g)),
      msg_types_(std::move(msg_types)) {
  CAF_ASSERT(f_);
  CAF_ASSERT(g_);
  // composed actor has dependency on constituent actors by default;
  // if either constituent actor is already dead upon establishing
  // the dependency, the actor is spawned dead
  f_->get()->attach(default_attachable::make_monitor(actor_cast<actor_addr>(f_),
                                                     address()));
  if (g_ != f_)
    g_->get()->attach(default_attachable::make_monitor(actor_cast<actor_addr>(g_),
                                                       address()));
}

void sequencer::enqueue(mailbox_element_ptr what, execution_unit* context) {
  auto down_msg_handler = [&](down_msg& dm) {
    // quit if either `f` or `g` are no longer available
    cleanup(std::move(dm.reason), context);
  };
  if (handle_system_message(*what, context, false, down_msg_handler))
    return;
  strong_actor_ptr f;
  strong_actor_ptr g;
  error err;
  shared_critical_section([&] {
    f = f_;
    g = g_;
    err = fail_state_;
  });
  if (!f) {
    // f and g are invalid only after the sequencer terminated
    bounce(what, err);
    return;
  }
  // process and forward the non-system message;
  // store `f` as the next stage in the forwarding chain
  what->stages.push_back(std::move(f));
  // forward modified message to `g`
  g->enqueue(std::move(what), context);
}

sequencer::message_types_set sequencer::message_types() const {
  return msg_types_;
}

void sequencer::on_cleanup(const error&) {
  f_.reset();
  g_.reset();
}

} // namespace decorator
} // namespace caf
