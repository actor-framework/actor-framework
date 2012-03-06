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


#include "cppa/event_based_actor.hpp"

namespace cppa {

void event_based_actor::become_void()
{
    m_loop_stack.clear();
}

void event_based_actor::do_become(behavior* bhvr, bool has_ownership)
{
    reset_timeout();
    request_timeout(bhvr->timeout());
    stack_element se{bhvr};
    if (!has_ownership) se.get_deleter().disable();
    // keep always the latest element in the stack to prevent subtle errors,
    // e.g., the addresses of all variables in a lambda expression calling
    // become() suddenly are invalid if we would pop the behavior!
    if (m_loop_stack.size() < 2)
    {
        m_loop_stack.push_back(std::move(se));
    }
    else
    {
        m_loop_stack[0] = std::move(m_loop_stack[1]);
        m_loop_stack[1] = std::move(se);
    }
}

} // namespace cppa
