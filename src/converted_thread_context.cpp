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

void converted_thread_context::dequeue(partial_function& rules)  /*override*/
{
    auto rm_fun = [&](mailbox_cache_element& node) { return dq(*node, rules); };
    auto& mbox_cache = m_mailbox.cache();
    auto mbox_end = mbox_cache.end();
    auto iter = std::find_if(mbox_cache.begin(), mbox_end, rm_fun);
    while (iter == mbox_end)
    {
        iter = std::find_if(m_mailbox.fetch_more(), mbox_end, rm_fun);
    }
    mbox_cache.erase(iter);
}

void converted_thread_context::dequeue(behavior& rules) /*override*/
{
    if (rules.timeout().valid())
    {
        auto timeout = now();
        timeout += rules.timeout();
        auto rm_fun = [&](mailbox_cache_element& node)
        {
            return dq(*node, rules.get_partial_function());
        };
        auto& mbox_cache = m_mailbox.cache();
        auto mbox_end = mbox_cache.end();
        auto iter = std::find_if(mbox_cache.begin(), mbox_end, rm_fun);
        while (iter == mbox_end)
        {
            auto next = m_mailbox.try_fetch_more(timeout);
            if (next == mbox_end)
            {
                rules.handle_timeout();
                return;
            }
            iter = std::find_if(next, mbox_end, rm_fun);
        }
        mbox_cache.erase(iter);
    }
    else
    {
        converted_thread_context::dequeue(rules.get_partial_function());
    }
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

bool converted_thread_context::dq(mailbox_element& node, partial_function& rules)
{
    if (   m_trap_exit == false
        && throw_on_exit(node.msg) == normal_exit_signal)
    {
        return false;
    }
    std::swap(m_last_dequeued, node.msg);
    std::swap(m_last_sender, node.sender);
    {
        mailbox_element::guard qguard{&node};
        if (rules(m_last_dequeued))
        {
            // client calls erase(iter)
            qguard.release();
            m_last_dequeued.reset();
            m_last_sender.reset();
            return true;
        }
    }
    // no match (restore members)
    std::swap(m_last_dequeued, node.msg);
    std::swap(m_last_sender, node.sender);
    return false;
}

} } // namespace cppa::detail
