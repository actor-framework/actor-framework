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
#include "cppa/resumable.hpp"
#include "cppa/actor_state.hpp"
#include "cppa/message_header.hpp"

#include "cppa/detail/yield_interface.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"

namespace cppa { namespace policy {

class cooperative_scheduling {

 public:

    using timeout_type = int;

    // this does return nullptr
    template<class Actor, typename F>
    void fetch_messages(Actor* self, F cb) {
        auto e = self->mailbox().try_pop();
        while (e == nullptr) {
            if (self->mailbox().can_fetch_more() == false) {
                self->set_state(actor_state::about_to_block);
                // make sure mailbox is empty
                if (self->mailbox().can_fetch_more()) {
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
            e = self->mailbox().try_pop();
        }
    }

    template<class Actor, typename F>
    inline void fetch_messages(Actor* self, F cb, timeout_type) {
        // a call to this call is always preceded by init_timeout,
        // which will trigger a timeout message
        fetch_messages(self, cb);
    }

    template<class Actor>
    inline void launch(Actor*) {
        static_cast<void>(m_hidden);
    }

    template<class Actor>
    void enqueue(Actor* self, const message_header& hdr, any_tuple& msg) {
        auto e = self->new_mailbox_element(hdr, std::move(msg));
        switch (self->mailbox().enqueue(e)) {
            case intrusive::first_enqueued: {
                auto state = self->state();
                auto set_ready = [&]() -> bool {
                    auto s = self->cas_state(state, actor_state::ready);
                    return s == actor_state::ready;
                };
                for (;;) {
                    switch (state) {
                        case actor_state::blocked: {
                            if (set_ready()) {
                                //m_scheduler->enqueue(this);
                                return;
                            }
                            break;
                        }
                        case actor_state::about_to_block: {
                            if (set_ready()) {
                                return;
                            }
                            break;
                        }
                        default: return;
                    }
                }
                break;
            }
            case intrusive::queue_closed: {
                if (hdr.id.is_request()) {
                    //FIXME
                    //detail::sync_request_bouncer f{exit_reason()};
                    //f(hdr.sender, hdr.id);
                }
                break;
            }
            default: break;
        }
    }

 private:

    // denotes whether this actor is ignored by await_all_actors_done()
    bool m_hidden;

};

} } // namespace cppa::policy

#endif // CPPA_COOPERATIVE_SCHEDULING_HPP
