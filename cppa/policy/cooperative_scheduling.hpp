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


#ifndef CPPA_POLICY_COOPERATIVE_SCHEDULING_HPP
#define CPPA_POLICY_COOPERATIVE_SCHEDULING_HPP

#include <atomic>

#include "cppa/message.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/singletons.hpp"
#include "cppa/message_header.hpp"

#include "cppa/detail/yield_interface.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"

namespace cppa {
namespace policy {

class cooperative_scheduling {

 public:

    using timeout_type = int;

    template<class Actor>
    inline void launch(Actor* self, execution_unit* host) {
        // detached in scheduler::worker::run
        self->attach_to_scheduler();
        if (host) host->exec_later(self);
        else get_scheduling_coordinator()->enqueue(self);
    }

    template<class Actor>
    void enqueue(Actor* self, msg_hdr_cref hdr,
                 message& msg, execution_unit* host) {
        auto e = self->new_mailbox_element(hdr, std::move(msg));
        switch (self->mailbox().enqueue(e)) {
            case intrusive::enqueue_result::unblocked_reader: {
                // re-schedule actor
                if (host) host->exec_later(self);
                else get_scheduling_coordinator()->enqueue(self);
                break;
            }
            case intrusive::enqueue_result::queue_closed: {
                if (hdr.id.is_request()) {
                    detail::sync_request_bouncer f{self->exit_reason()};
                    f(hdr.sender, hdr.id);
                }
                break;
            }
            case intrusive::enqueue_result::success:
                // enqueued to a running actors' mailbox; nothing to do
                break;
        }
    }

};

} // namespace policy
} // namespace cppa

#endif // CPPA_POLICY_COOPERATIVE_SCHEDULING_HPP
