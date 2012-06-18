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

abstract_event_based_actor::abstract_event_based_actor() : super(super::blocked) {
}

void abstract_event_based_actor::dequeue(behavior&) {
    quit(exit_reason::unallowed_function_call);
}

void abstract_event_based_actor::dequeue(partial_function&) {
    quit(exit_reason::unallowed_function_call);
}

auto abstract_event_based_actor::handle_message(mailbox_element& node) -> handle_message_result {
    CPPA_REQUIRE(node.marked == false);
    CPPA_REQUIRE(m_loop_stack.empty() == false);
    auto& bhvr = *(m_loop_stack.back());
    switch (filter_msg(node.msg)) {
        case detail::normal_exit_signal:
        case detail::expired_timeout_message:
            return drop_msg;

        case detail::timeout_message:
            m_has_pending_timeout_request = false;
            CPPA_REQUIRE(bhvr.timeout().valid());
            bhvr.handle_timeout();
            if (!m_loop_stack.empty()) {
                auto& next_bhvr = *(m_loop_stack.back());
                request_timeout(next_bhvr.timeout());
            }
            return msg_handled;

        default:
            break;
    }
    std::swap(m_last_dequeued, node.msg);
    std::swap(m_last_sender, node.sender);
    if ((bhvr.get_partial_function())(m_last_dequeued)) {
        m_last_dequeued.reset();
        m_last_sender.reset();
        // we definitely don't have a pending timeout now
        m_has_pending_timeout_request = false;
        return msg_handled;
    }
    // no match, restore members
    std::swap(m_last_dequeued, node.msg);
    std::swap(m_last_sender, node.sender);
    return cache_msg;
}

resume_result abstract_event_based_actor::resume(util::fiber*) {
    auto done_cb = [&]() {
        m_state.store(abstract_scheduled_actor::done);
        m_loop_stack.clear();
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
                switch (handle_message(*e)) {
                    case drop_msg: {
                        release_node(e);
                        break; // nop
                    }
                    case msg_handled: {
                        release_node(e);
                        if (m_loop_stack.empty()) {
                            done_cb();
                            return resume_result::actor_done;
                        }
                        // try to match cached messages before receiving new ones
                        auto i = m_cache.begin();
                        while (i != m_cache.end()) {
                            switch (handle_message(*(*i))) {
                                case drop_msg: {
                                    release_node(i->release());
                                    i = m_cache.erase(i);
                                    break;
                                }
                                case msg_handled: {
                                    release_node(i->release());
                                    m_cache.erase(i);
                                    if (m_loop_stack.empty()) {
                                        done_cb();
                                        return resume_result::actor_done;
                                    }
                                    i = m_cache.begin();
                                    break;
                                }
                                case cache_msg: {
                                    ++i;
                                    break;
                                }
                                default: CPPA_CRITICAL("illegal result of handle_message");
                            }
                        }
                        break;
                    }
                    case cache_msg: {
                        m_cache.emplace_back(e);
                        break;
                    }
                    default: CPPA_CRITICAL("illegal result of handle_message");
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
