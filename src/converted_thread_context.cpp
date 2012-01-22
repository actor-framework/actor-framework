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


#include <memory>
#include <algorithm>

#include "cppa/self.hpp"
#include "cppa/exception.hpp"
#include "cppa/detail/matches.hpp"
#include "cppa/detail/invokable.hpp"
#include "cppa/detail/intermediate.hpp"
#include "cppa/detail/converted_thread_context.hpp"

namespace cppa { namespace detail {

converted_thread_context::converted_thread_context()
    : m_exit_msg_pattern(atom(":Exit"))
{
}

void converted_thread_context::quit(std::uint32_t reason)
{
    super::cleanup(reason);
    // actor_exited should not be catched, but if anyone does,
    // self must point to a newly created instance
    //self.set(nullptr);
    throw actor_exited(reason);
}

void converted_thread_context::cleanup(std::uint32_t reason)
{
    super::cleanup(reason);
}

void converted_thread_context::enqueue(actor* sender, any_tuple&& msg)
{
    m_mailbox.push_back(fetch_node(sender, std::move(msg)));
}

void converted_thread_context::enqueue(actor* sender, const any_tuple& msg)
{
    m_mailbox.push_back(fetch_node(sender, msg));
}

void converted_thread_context::dequeue(invoke_rules& rules) /*override*/
{
    queue_node_buffer buffer;
    queue_node_ptr node(m_mailbox.pop());
    while (dq(node, rules, buffer) == false)
    {
        node.reset(m_mailbox.pop());
    }
}

void converted_thread_context::dequeue(timed_invoke_rules& rules)  /*override*/
{
    auto timeout = now();
    timeout += rules.timeout();
    queue_node_buffer buffer;
    queue_node_ptr node(m_mailbox.try_pop());
    do
    {
        if (!node)
        {
            node.reset(m_mailbox.try_pop(timeout));
            if (!node)
            {
                if (!buffer.empty()) m_mailbox.push_front(std::move(buffer));
                rules.handle_timeout();
                return;
            }
        }
    }
    while (dq(node, rules, buffer) == false);
}
converted_thread_context::throw_on_exit_result
converted_thread_context::throw_on_exit(any_tuple const& msg)
{
    if (matches(msg, m_exit_msg_pattern))
    {
        auto reason = msg.get_as<std::uint32_t>(1);
        if (reason != exit_reason::normal)
        {
            // throws
            quit(reason);
        }
        else
        {
            return normal_exit_signal;
        }
    }
    return not_an_exit_signal;
}

bool converted_thread_context::dq(queue_node_ptr& node,
                                  invoke_rules_base& rules,
                                  queue_node_buffer& buffer)
{
    if (m_trap_exit == false && throw_on_exit(node->msg) == normal_exit_signal)
    {
        return false;
    }
    auto imd = rules.get_intermediate(node->msg);
    if (imd)
    {
        m_last_dequeued = std::move(node->msg);
        m_last_sender = std::move(node->sender);
        // restore mailbox before invoking imd
        if (!buffer.empty()) m_mailbox.push_front(std::move(buffer));
        imd->invoke();
        return true;
    }
    else
    {
        buffer.push_back(node.release());
        return false;
    }
}

} } // namespace cppa::detail
