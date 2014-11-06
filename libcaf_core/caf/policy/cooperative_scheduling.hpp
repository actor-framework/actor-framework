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

#ifndef CAF_POLICY_COOPERATIVE_SCHEDULING_HPP
#define CAF_POLICY_COOPERATIVE_SCHEDULING_HPP

#include <atomic>

#include "caf/message.hpp"
#include "caf/execution_unit.hpp"

#include "caf/scheduler/abstract_coordinator.hpp"

#include "caf/detail/singletons.hpp"
#include "caf/detail/single_reader_queue.hpp"

namespace caf {
namespace policy {

class cooperative_scheduling {

 public:

  using timeout_type = int;

  template <class Actor>
  inline void launch(Actor* self, execution_unit* host) {
    // detached in scheduler::worker::run
    self->attach_to_scheduler();
    if (host)
      host->exec_later(self);
    else
      detail::singletons::get_scheduling_coordinator()->enqueue(self);
  }

  template <class Actor>
  void enqueue(Actor* self, const actor_addr& sender, message_id mid,
         message& msg, execution_unit* eu) {
    auto e = self->new_mailbox_element(sender, mid, std::move(msg));
    switch (self->mailbox().enqueue(e)) {
      case detail::enqueue_result::unblocked_reader: {
        // re-schedule actor
        if (eu)
          eu->exec_later(self);
        else
          detail::singletons::get_scheduling_coordinator()->enqueue(
            self);
        break;
      }
      case detail::enqueue_result::queue_closed: {
        if (mid.is_request()) {
          detail::sync_request_bouncer f{self->exit_reason()};
          f(sender, mid);
        }
        break;
      }
      case detail::enqueue_result::success:
        // enqueued to a running actors' mailbox; nothing to do
        break;
    }
  }

};

} // namespace policy
} // namespace caf

#endif // CAF_POLICY_COOPERATIVE_SCHEDULING_HPP
