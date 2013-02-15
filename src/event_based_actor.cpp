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

namespace cppa {

event_based_actor::event_based_actor() : super(super::blocked) { }

void event_based_actor::dequeue(behavior&) {
    quit(exit_reason::unallowed_function_call);
}

void event_based_actor::dequeue(partial_function&) {
    quit(exit_reason::unallowed_function_call);
}

void event_based_actor::dequeue_response(behavior&, message_id_t) {
    quit(exit_reason::unallowed_function_call);
}

resume_result event_based_actor::resume(util::fiber*, actor_ptr& next_job) {
#   ifdef CPPA_DEBUG
    auto st = m_state.load();
    switch (st) {
        case abstract_scheduled_actor::ready:
        case abstract_scheduled_actor::pending:
            // state ok
            break;
        default: {
            std::cerr << "UNEXPECTED STATE: " << st << std::endl;
            // failed requirement calls abort() and prints stack trace
            CPPA_REQUIRE(   st == abstract_scheduled_actor::ready
                         || st == abstract_scheduled_actor::pending);
        }
    }
#   endif // CPPA_DEBUG
    scoped_self_setter sss{this};
    auto done_cb = [&]() {
        m_state.store(abstract_scheduled_actor::done);
        m_bhvr_stack.clear();
        m_bhvr_stack.cleanup();
        on_exit();
        CPPA_REQUIRE(next_job == nullptr);
        next_job.swap(m_chained_actor);
    };
    CPPA_REQUIRE(next_job == nullptr);
    try {
        detail::recursive_queue_node* e = nullptr;
        for (;;) {
            e = m_mailbox.try_pop();
            if (e == nullptr) {
                CPPA_REQUIRE(next_job == nullptr);
                next_job.swap(m_chained_actor);
                m_state.store(abstract_scheduled_actor::about_to_block);
                std::atomic_thread_fence(std::memory_order_seq_cst);
                if (m_mailbox.can_fetch_more() == false) {
                    switch (compare_exchange_state(
                                abstract_scheduled_actor::about_to_block,
                                abstract_scheduled_actor::blocked)) {
                        case abstract_scheduled_actor::ready:
                            // interrupted by arriving message
                            // restore members
                            CPPA_REQUIRE(m_chained_actor == nullptr);
                            next_job.swap(m_chained_actor);
                            break;
                        case abstract_scheduled_actor::blocked:
                            // done setting actor to blocked
                            return resume_result::actor_blocked;
                        case abstract_scheduled_actor::pending:
                            CPPA_CRITICAL("illegal state: pending");
                        case abstract_scheduled_actor::done:
                            CPPA_CRITICAL("illegal state: done");
                        case abstract_scheduled_actor::about_to_block:
                            CPPA_CRITICAL("illegal state: about_to_block");
                        default:
                            CPPA_CRITICAL("invalid state");
                    };
                }
                else {
                    m_state.store(abstract_scheduled_actor::ready);
                    CPPA_REQUIRE(m_chained_actor == nullptr);
                    next_job.swap(m_chained_actor);
                }
            }
            else if (m_bhvr_stack.invoke(m_policy, this, e)) {
                if (m_bhvr_stack.empty()) {
                    done_cb();
                    return resume_result::actor_done;
                }
                m_bhvr_stack.cleanup();
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

void event_based_actor::do_become(behavior&& bhvr, bool discard_old) {
    reset_timeout();
    request_timeout(bhvr.timeout());
    if (discard_old) m_bhvr_stack.pop_async_back();
    m_bhvr_stack.push_back(std::move(bhvr));
}

void event_based_actor::become_waiting_for(behavior&& bhvr, message_id_t mf) {
    if (bhvr.timeout().valid()) {
        reset_timeout();
        request_timeout(bhvr.timeout());
    }
    m_bhvr_stack.push_back(std::move(bhvr), mf);
}

void event_based_actor::quit(std::uint32_t reason) {
    if (reason == exit_reason::normal) {
        cleanup(exit_reason::normal);
        m_bhvr_stack.clear();
    }
    else {
        abstract_scheduled_actor::quit(reason);
    }
}

scheduled_actor_type event_based_actor::impl_type() {
    return event_based_impl;
}

} // namespace cppa
