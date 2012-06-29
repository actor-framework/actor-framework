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
#include "cppa/event_based_actor.hpp"

#include "cppa/detail/filter_result.hpp"

namespace cppa {

event_based_actor::event_based_actor() : super(super::blocked) { }

void event_based_actor::dequeue(behavior&) {
    quit(exit_reason::unallowed_function_call);
}

void event_based_actor::dequeue(partial_function&) {
    quit(exit_reason::unallowed_function_call);
}

resume_result event_based_actor::resume(util::fiber*) {
    scoped_self_setter sss{this};
    auto done_cb = [&]() {
        m_state.store(abstract_scheduled_actor::done);
        m_bhvr_stack.clear();
        on_exit();
    };
    try {
        detail::recursive_queue_node* e;
        for (;;) {
            e = m_mailbox.try_pop();
            if (!e) {
                m_state.store(abstract_scheduled_actor::about_to_block);
                if (m_mailbox.can_fetch_more() == false) {
                    switch (compare_exchange_state(
                                abstract_scheduled_actor::about_to_block,
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
                if (m_policy.invoke(this, e, get_behavior())) {
                    // try to match cached message before receiving new ones
                    do {
                        m_bhvr_stack.cleanup();
                        if (m_bhvr_stack.empty()) {
                            done_cb();
                            return resume_result::actor_done;
                        }
                    } while (m_policy.invoke_from_cache(this, get_behavior()));
                }
            }
        }
    }
    catch (actor_exited& what) { cleanup(what.reason()); }
    catch (...)                { cleanup(exit_reason::unhandled_exception); }
    done_cb();
    return resume_result::actor_done;
}

bool event_based_actor::has_behavior() {
    return m_bhvr_stack.empty() == false;
}

void event_based_actor::do_become(behavior* ptr, bool owner, bool discard_old) {
    reset_timeout();
    request_timeout(ptr->timeout());
    if (discard_old) m_bhvr_stack.pop_back();
    m_bhvr_stack.push_back(ptr, owner);
}

void event_based_actor::quit(std::uint32_t reason) {
    if (reason == exit_reason::normal) {
        cleanup(exit_reason::normal);
        m_bhvr_stack.clear();
        m_bhvr_stack.cleanup();
    }
    else {
        abstract_scheduled_actor::quit(reason);
    }
}

void event_based_actor::unbecome() {
    m_bhvr_stack.pop_back();
}

scheduled_actor_type event_based_actor::impl_type() {
    return event_based_impl;
}

} // namespace cppa
