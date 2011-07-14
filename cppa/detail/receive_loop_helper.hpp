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
 * Copyright (C) 2011, Dominik Charousset <dominik.charousset@haw-hamburg.de> *
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

#ifndef RECEIVE_LOOP_HELPER_HPP
#define RECEIVE_LOOP_HELPER_HPP

#include "cppa/invoke_rules.hpp"

// forward declaration
namespace cppa { void receive(invoke_rules&); }

namespace cppa { namespace detail {

template<typename Statement>
void receive_while_loop(Statement& stmt, invoke_rules& rules)
{
    while (stmt())
    {
        receive(rules);
    }
}

template<typename Statement>
void receive_until_loop(Statement& stmt, invoke_rules& rules)
{
    do
    {
        receive(rules);
    }
    while (stmt() == false);
}

template<typename Statement, void (*Loop)(Statement&, invoke_rules&)>
struct receive_loop_helper
{
    Statement m_stmt;
    receive_loop_helper(Statement&& stmt) : m_stmt(std::move(stmt))
    {
    }
    void operator()(invoke_rules& rules)
    {
        Loop(m_stmt, rules);
    }
    void operator()(invoke_rules&& rules)
    {
        invoke_rules tmp(std::move(rules));
        (*this)(tmp);
    }
    template<typename Arg0, typename... Args>
    void operator()(invoke_rules& rules, Arg0&& arg0, Args&&... args)
    {
        (*this)(rules.splice(std::forward<Arg0>(arg0)),
                std::forward<Args>(args)...);
    }
    template<typename Arg0, typename... Args>
    void operator()(invoke_rules&& rules, Arg0&& arg0, Args&&... args)
    {
        invoke_rules tmp(std::move(rules));
        (*this)(tmp.splice(std::forward<Arg0>(arg0)),
                std::forward<Args>(args)...);
    }
};

} } // namespace cppa::detail

#endif // RECEIVE_LOOP_HELPER_HPP
