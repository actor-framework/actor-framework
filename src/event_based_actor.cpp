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

namespace {

template<class StackElement, class Vec, class What>
void push_to(Vec& vec, What* bhvr, bool has_ownership)
{
    // keep always the latest element in the stack to prevent subtle errors,
    // e.g., the addresses of all variables in a lambda expression calling
    // become() suddenly are invalid if we would pop the behavior!
    if (vec.size() < 2)
    {
        vec.push_back(StackElement(bhvr, has_ownership));
    }
    else
    {
        vec[0] = std::move(vec[1]);
        vec[1] = StackElement(bhvr, has_ownership);
    }
}

} // namespace <anonymous>

namespace cppa {

void event_based_actor::become_void()
{
    m_loop_stack.clear();
}

void event_based_actor::do_become(behavior* bhvr)
{
    if (bhvr->is_left())
    {
        do_become(&(bhvr->left()), false);
    }
    else
    {
        do_become(&(bhvr->right()), false);
    }
}

void event_based_actor::do_become(invoke_rules* bhvr, bool has_ownership)
{
    reset_timeout();
    push_to<stack_element>(m_loop_stack, bhvr, has_ownership);
}

void event_based_actor::do_become(timed_invoke_rules* bhvr, bool has_ownership)
{
    request_timeout(bhvr->timeout());
    push_to<stack_element>(m_loop_stack, bhvr, has_ownership);
}

} // namespace cppa
