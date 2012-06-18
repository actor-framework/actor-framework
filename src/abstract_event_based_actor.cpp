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
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
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


#include <iostream>
#include "cppa/to_string.hpp"

#include "cppa/self.hpp"
#include "cppa/abstract_event_based_actor.hpp"

#include "cppa/detail/filter_result.hpp"

namespace cppa {

abstract_event_based_actor::abstract_event_based_actor()
: super(super::blocked) { }

void abstract_event_based_actor::dequeue(behavior&) {
    quit(exit_reason::unallowed_function_call);
}

void abstract_event_based_actor::dequeue(partial_function&) {
    quit(exit_reason::unallowed_function_call);
}

resume_result abstract_event_based_actor::resume(util::fiber*) {
    auto done_cb = [&]() {
        m_state.store(abstract_scheduled_actor::done);
        m_behavior_stack.clear();
        on_exit();
    };
    self.set(this);
    try {
        detail::recursive_queue_node* e;
        for (;;) {
            e = m_mailbox.try_pop();
            if (!e) {
                m_state.store(abstract_scheduled_actor::about_to_block);
                if (m_mailbox.can_fetch_more() == false) {
                    switch (compare_exchange_state(abstract_scheduled_actor::about_to_block,
                                                   abstract_scheduled_actor::blocked)) {
                        case abstract_scheduled_actor::ready: {
                            break;
                        }
                        case abstract_scheduled_actor::blocked: {
                            return resume_result::actor_blocked;
                        }
                        default: CPPA_CRITICAL("illegal actor state");
                    };
                }
            }
            else {
                if (m_recv_policy.invoke(this, e, current_behavior())) {
                    // try to match cached message before receiving new ones
                    do {
                        if (m_behavior_stack.empty()) {
                            done_cb();
                            return resume_result::actor_done;
                        }
                    } while (m_recv_policy.invoke_from_cache(this, current_behavior()));
                }
            }
        }
    }
    catch (actor_exited& what) {
        cleanup(what.reason());
    }
    catch (...) {
        cleanup(exit_reason::unhandled_exception);
    }
    done_cb();
    return resume_result::actor_done;
}

void abstract_event_based_actor::on_exit() {
}

} // namespace cppa
