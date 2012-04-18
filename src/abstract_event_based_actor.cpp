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
#include "cppa/detail/invokable.hpp"
#include "cppa/abstract_event_based_actor.hpp"

namespace cppa {

abstract_event_based_actor::abstract_event_based_actor()
    : super(super::blocked)
{
    //m_mailbox_pos = m_mailbox.cache().end();
}

void abstract_event_based_actor::dequeue(behavior&)
{
    quit(exit_reason::unallowed_function_call);
}

void abstract_event_based_actor::dequeue(partial_function&)
{
    quit(exit_reason::unallowed_function_call);
}

bool abstract_event_based_actor::handle_message(mailbox_element& node)
{
    CPPA_REQUIRE(m_loop_stack.empty() == false);
    if (node.marked) return false;
    auto& bhvr = *(m_loop_stack.back());
    switch (filter_msg(node.msg))
    {
        case normal_exit_signal:
        case expired_timeout_message:
            node.marked = true;
            return false;

        case timeout_message:
            m_has_pending_timeout_request = false;
            CPPA_REQUIRE(bhvr.timeout().valid());
            bhvr.handle_timeout();
            if (!m_loop_stack.empty())
            {
                auto& next_bhvr = *(m_loop_stack.back());
                request_timeout(next_bhvr.timeout());
            }
            return true;

        default:
            break;
    }
    std::swap(m_last_dequeued, node.msg);
    std::swap(m_last_sender, node.sender);
    //m_last_dequeued = node.msg;
    //m_last_sender = node.sender;
    // make sure no timeout is handled incorrectly in a nested receive
    ++m_active_timeout_id;
    if ((bhvr.get_partial_function())(m_last_dequeued))
    {
        node.marked = true;
        m_last_dequeued.reset();
        m_last_sender.reset();
        // we definitely don't have a pending timeout now
        m_has_pending_timeout_request = false;
        return true;
    }
    // no match, restore members
    --m_active_timeout_id;
    std::swap(m_last_dequeued, node.msg);
    std::swap(m_last_sender, node.sender);
    return false;
}

void abstract_event_based_actor::resume(util::fiber*, scheduler::callback* cb)
{
    self.set(this);
    auto& mbox_cache = m_mailbox.cache();
    auto pos = mbox_cache.end();
    try
    {
        for (;;)
        {
            if (m_loop_stack.empty())
            {
                cleanup(exit_reason::normal);
                m_state.store(abstract_scheduled_actor::done);
                m_loop_stack.clear();
                on_exit();
                cb->exec_done();
                return;
            }
            while (pos == mbox_cache.end())
            {
                // try fetch more
                if (m_mailbox.can_fetch_more() == false)
                {
                    // sweep marked elements
                    auto new_end = std::remove_if(mbox_cache.begin(), mbox_cache.end(),
                                                  [](detail::recursive_queue_node const& n) { return n.marked; });
                    mbox_cache.resize(std::distance(mbox_cache.begin(), new_end));
                    m_state.store(abstract_scheduled_actor::about_to_block);
                    CPPA_MEMORY_BARRIER();
                    if (m_mailbox.can_fetch_more() == false)
                    {
                        switch (compare_exchange_state(abstract_scheduled_actor::about_to_block,
                                                       abstract_scheduled_actor::blocked))
                        {
                            case abstract_scheduled_actor::ready:
                            {
                                // someone preempt us, set position to new end()
                                pos = mbox_cache.end();
                                break;
                            }
                            case abstract_scheduled_actor::blocked:
                            {
                                return;
                            }
                            default: exit(7); // illegal state
                        };
                    }
                }
                pos = m_mailbox.try_fetch_more();
            }
            pos = std::find_if(pos, mbox_cache.end(),
                               [&](mailbox_element& e) { return handle_message(e); });
            if (pos != mbox_cache.end())
            {
                // handled a message, scan mailbox from start again
                pos = mbox_cache.begin();
            }
        }
    }
    catch (actor_exited& what)
    {
        cleanup(what.reason());
    }
    catch (...)
    {
        cleanup(exit_reason::unhandled_exception);
    }
    m_state.store(abstract_scheduled_actor::done);
    m_loop_stack.clear();
    on_exit();
    cb->exec_done();
}

void abstract_event_based_actor::on_exit()
{
}

} // namespace cppa
