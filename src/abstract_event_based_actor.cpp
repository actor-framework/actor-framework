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
    : super(abstract_event_based_actor::blocked)
{
    m_mailbox_pos = m_mailbox.cache().end();
}

void abstract_event_based_actor::dequeue(behavior&)
{
    quit(exit_reason::unallowed_function_call);
}

void abstract_event_based_actor::dequeue(partial_function&)
{
    quit(exit_reason::unallowed_function_call);
}

bool abstract_event_based_actor::handle_message(queue_node& node)
{
    CPPA_REQUIRE(m_loop_stack.empty() == false);
    auto& bhvr = *(m_loop_stack.back());
    if (bhvr.timeout().valid())
    {
        switch (dq(node, bhvr.get_partial_function()))
        {
            case dq_timeout_occured:
            {
                bhvr.handle_timeout();
                // fall through
            }
            case dq_done:
            {
                // callback might have called become()/unbecome()
                // request next timeout if needed
                if (!m_loop_stack.empty())
                {
                    auto& next_bhvr = *(m_loop_stack.back());
                    request_timeout(next_bhvr.timeout());
                }
                return true;
            }
            default: return false;
        }
    }
    else
    {
        return dq(node, bhvr.get_partial_function()) == dq_done;
    }
}

bool abstract_event_based_actor::invoke_from_cache()
{
    for (auto i = m_mailbox_pos; i != m_mailbox.cache().end(); ++i)
    {
        if (handle_message(*(*i)))
        {
            m_mailbox.cache().erase(i);
            return true;
        }
    }
    return false;
}

void abstract_event_based_actor::resume(util::fiber*, resume_callback* callback)
{
    self.set(this);
    auto done_cb = [=]()
    {
        m_state.store(abstract_scheduled_actor::done);
        while (!m_loop_stack.empty()) m_loop_stack.pop_back();
        on_exit();
        callback->exec_done();
    };
    auto& mbox_cache = m_mailbox.cache();
    auto mbox_end = mbox_cache.end();
    /*auto rm_fun = [=](queue_node_ptr& ptr) -> bool
    {
        CPPA_REQUIRE(ptr.get() != nullptr);
        return handle_message(*ptr);
    };*/
    try
    {
        for (;;)
        {
            if (m_loop_stack.empty())
            {
                cleanup(exit_reason::normal);
                done_cb();
                return;
            }
            while (m_mailbox_pos == mbox_end)
            {
                // try fetch more
                if (m_mailbox.can_fetch_more() == false)
                {
                    m_state.store(abstract_scheduled_actor::about_to_block);
                    CPPA_MEMORY_BARRIER();
                    if (m_mailbox.can_fetch_more() == false)
                    {
                        switch (compare_exchange_state(abstract_scheduled_actor::about_to_block,
                                                       abstract_scheduled_actor::blocked))
                        {
                            case abstract_scheduled_actor::ready:
                            {
                                // someone preempt us
                                break;
                            }
                            case abstract_scheduled_actor::blocked:
                            {
                                // done
                                return;
                            }
                            default: exit(7); // illegal state
                        };
                    }
                }
                m_mailbox_pos = m_mailbox.try_fetch_more();
            }
            m_mailbox_pos = (invoke_from_cache()) ? mbox_cache.begin()
                                                  : mbox_cache.end();
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
    done_cb();
}

void abstract_event_based_actor::on_exit()
{
}

} // namespace cppa
