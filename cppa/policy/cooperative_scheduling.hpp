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
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#ifndef CPPA_COOPERATIVE_SCHEDULING_HPP
#define CPPA_COOPERATIVE_SCHEDULING_HPP

#include <atomic>

#include "cppa/any_tuple.hpp"
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
                 any_tuple& msg, execution_unit* host) {
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


#endif // CPPA_COOPERATIVE_SCHEDULING_HPP
