/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

#ifndef CAF_POLICY_COOPERATIVE_SCHEDULING_HPP
#define CAF_POLICY_COOPERATIVE_SCHEDULING_HPP

#include <atomic>

#include "caf/message.hpp"
#include "caf/scheduler.hpp"

#include "caf/detail/singletons.hpp"
#include "caf/detail/single_reader_queue.hpp"

namespace caf {
namespace policy {

class cooperative_scheduling {

 public:

    using timeout_type = int;

    template<class Actor>
    inline void launch(Actor* self, execution_unit* host) {
        // detached in scheduler::worker::run
        self->attach_to_scheduler();
        if (host)
            host->exec_later(self);
        else
            detail::singletons::get_scheduling_coordinator()->enqueue(self);
    }

    template<class Actor>
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
