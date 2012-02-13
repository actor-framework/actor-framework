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


#include "cppa/self.hpp"
#include "cppa/detail/invokable.hpp"
#include "cppa/abstract_event_based_actor.hpp"

namespace cppa {

abstract_event_based_actor::abstract_event_based_actor()
    : super(abstract_event_based_actor::blocked)
{
}

void abstract_event_based_actor::dequeue(invoke_rules&)
{
    quit(exit_reason::unallowed_function_call);
}

void abstract_event_based_actor::dequeue(timed_invoke_rules&)
{
    quit(exit_reason::unallowed_function_call);
}

void abstract_event_based_actor::handle_message(queue_node_ptr& node,
                                                invoke_rules& behavior)
{
    // no need to handle result
    (void) dq(node, behavior, m_buffer);
}

void abstract_event_based_actor::handle_message(queue_node_ptr& node,
                                                timed_invoke_rules& behavior)
{
    switch (dq(node, behavior, m_buffer))
    {
        case dq_timeout_occured:
        {
            behavior.handle_timeout();
            // fall through
        }
        case dq_done:
        {
            // callback might have called become()/unbecome()
            // request next timeout if needed
            if (!m_loop_stack.empty())
            {
                auto& back = m_loop_stack.back();
                if (back.is_right())
                {
                    request_timeout(back.right()->timeout());
                }
            }
            break;
        }
        default: break;
    }
}

void abstract_event_based_actor::handle_message(queue_node_ptr& node)
{
    auto& bhvr = m_loop_stack.back();
    if (bhvr.is_left())
    {
        handle_message(node, *(bhvr.left()));
    }
    else
    {
        handle_message(node, *(bhvr.right()));
    }
}

void abstract_event_based_actor::resume(util::fiber*, resume_callback* callback)
{
    self.set(this);
    auto done_cb = [&]()
    {
        m_state.store(abstract_scheduled_actor::done);
        while (!m_loop_stack.empty()) m_loop_stack.pop_back();
        on_exit();
        callback->exec_done();
    };

    queue_node_ptr node;
    for (;;)
    //do
    {
        if (m_loop_stack.empty())
        {
            cleanup(exit_reason::normal);
            done_cb();
            return;
        }
        else if (m_mailbox.empty())
        {
            m_state.store(abstract_scheduled_actor::about_to_block);
            CPPA_MEMORY_BARRIER();
            if (!m_mailbox.empty())
            {
                // someone preempt us
                m_state.store(abstract_scheduled_actor::ready);
            }
            else
            {
                // nothing to do (wait for new messages)
                switch (compare_exchange_state(abstract_scheduled_actor::about_to_block,
                                               abstract_scheduled_actor::blocked))
                {
                    case abstract_scheduled_actor::ready:
                    {
                        // got a new job
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
        node.reset(m_mailbox.pop());
        try
        {
            handle_message(node);
        }
        catch (actor_exited& what)
        {
            cleanup(what.reason());
            done_cb();
            return;
        }
        catch (...)
        {
            cleanup(exit_reason::unhandled_exception);
            done_cb();
            return;
        }
    }
    //while (callback->still_ready());
}

void abstract_event_based_actor::on_exit()
{
}

} // namespace cppa
