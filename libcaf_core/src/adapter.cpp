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

#include "caf/decorator/adapter.hpp"

#include "caf/sec.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_system.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/system_messages.hpp"
#include "caf/default_attachable.hpp"

#include "caf/detail/disposer.hpp"
#include "caf/detail/merged_tuple.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

namespace caf {
namespace decorator {

adapter::adapter(strong_actor_ptr decorated, message msg)
    : monitorable_actor(actor_config{}.add_flag(is_actor_bind_decorator_flag)),
      decorated_(std::move(decorated)),
      merger_(std::move(msg)) {
  // bound actor has dependency on the decorated actor by default;
  // if the decorated actor is already dead upon establishing the
  // dependency, the actor is spawned dead
  decorated_->get()->attach(
    default_attachable::make_monitor(decorated_->get()->address(), address()));
}

void adapter::enqueue(mailbox_element_ptr x, execution_unit* context) {
  CAF_ASSERT(x);
  auto down_msg_handler = [&](down_msg& dm) {
    cleanup(std::move(dm.reason), context);
  };
  if (handle_system_message(*x, context, false, down_msg_handler))
    return;
  strong_actor_ptr decorated;
  message merger;
  error fail_state;
  shared_critical_section([&] {
    decorated = decorated_;
    merger = merger_;
    fail_state = fail_state_;
  });
  if (! decorated) {
    bounce(x, fail_state);
    return;
  }
  message tmp{detail::merged_tuple::make(merger, x->move_content_to_message())};
  decorated->enqueue(make_mailbox_element(std::move(x->sender), x->mid,
                                          std::move(x->stages), std::move(tmp)),
                     context);
}

void adapter::on_cleanup() {
  decorated_.reset();
  merger_.reset();
}

} // namespace decorator
} // namespace caf
