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

namespace cppa { namespace policy {

class cooperative_scheduling {

 public:

    using timeout_type = int;

    template<class Actor>
    timeout_type init_timeout(Actor* self, const util::duration& rel_time) {
        // request explicit timeout message
        self->request_timeout(rel_time);
        return 0; // return some dummy value
    }

    // this does return nullptr
    template<class Actor, typename F>
    void fetch_messages(Actor* self, F cb) {
        auto e = self->m_mailbox.try_pop();
        while (e == nullptr) {
            if (self->m_mailbox.can_fetch_more() == false) {
                self->set_state(actor_state::about_to_block);
                // make sure mailbox is empty
                if (self->m_mailbox.can_fetch_more()) {
                    // someone preempt us => continue
                    self->set_state(actor_state::ready);
                }
                // wait until actor becomes rescheduled
                else detail::yield(detail::yield_state::blocked);
            }
        }
        // ok, we have at least one message
        while (e) {
            cb(e);
            e = self->m_mailbox.try_pop();
        }
    }

    template<class Actor, typename F>
    inline void fetch_messages(Actor* self, F cb, timeout_type) {
        // a call to this call is always preceded by init_timeout,
        // which will trigger a timeout message
        fetch_messages(self, cb);
    }

};

} } // namespace cppa::policy

#endif // CPPA_COOPERATIVE_SCHEDULING_HPP
