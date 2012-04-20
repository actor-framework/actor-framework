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
#include <iostream>
#include <algorithm>

#include "cppa/self.hpp"
#include "cppa/exception.hpp"
#include "cppa/detail/matches.hpp"
#include "cppa/detail/invokable.hpp"
#include "cppa/detail/converted_thread_context.hpp"

namespace cppa { namespace detail {

converted_thread_context::converted_thread_context()
    : m_exit_msg_pattern(atom(":Exit")), m_invoke(this, this)
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

void converted_thread_context::enqueue(actor* sender, any_tuple msg)
{
    m_mailbox.push_back(fetch_node(sender, std::move(msg)));
}

void converted_thread_context::dequeue(partial_function& fun) // override
{
    if (m_invoke.invoke_from_cache(fun) == false)
    {
        queue_node_ptr e{m_mailbox.pop()};
        while (m_invoke.invoke(e, fun) == false)
        {
            e.reset(m_mailbox.pop());
        }
    }
}

void converted_thread_context::dequeue(behavior& bhvr) // override
{
    auto& fun = bhvr.get_partial_function();
    if (bhvr.timeout().valid() == false)
    {
        dequeue(fun);
        return;
    }
    if (m_invoke.invoke_from_cache(fun) == false)
    {
        auto timeout = now();
        timeout += bhvr.timeout();
        queue_node_ptr e{m_mailbox.try_pop(timeout)};
        while (e)
        {
            if (m_invoke.invoke(e, fun)) return;
            else e.reset(m_mailbox.try_pop(timeout));
        }
        bhvr.handle_timeout();
    }
}

} } // namespace cppa::detail
