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
struct receive_while_helper
{
    Statement m_stmt;
    receive_while_helper(Statement&& stmt) : m_stmt(std::move(stmt))
    {
    }
    void operator()(invoke_rules& rules)
    {
        while (m_stmt())
        {
            receive(rules);
        }
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

class do_receive_helper
{

    invoke_rules m_rules;

    inline void init(invoke_rules&) { }

    template<typename Arg0, typename... Args>
    inline void init(invoke_rules& rules, Arg0&& arg0, Args&&... args)
    {
        init(rules.splice(arg0), std::forward<Args>(args)...);
    }

 public:

    //template<typename... Args>
    //do_receive_helper(invoke_rules& rules, Args&&... args) : m_rules(std::move(rules))
    //{
    //    init(m_rules, std::forward<Args>(args)...);
    //}

    template<typename... Args>
    do_receive_helper(invoke_rules&& rules, Args&&... args) : m_rules(std::move(rules))
    {
        init(m_rules, std::forward<Args>(args)...);
    }

    template<typename Statement>
    void until(Statement&& stmt)
    {
        static_assert(std::is_same<bool, decltype(stmt())>::value,
                      "functor or function does not return a boolean");
        do
        {
            receive(m_rules);
        }
        while (stmt() == false);
    }

};

} } // namespace cppa::detail

#endif // RECEIVE_LOOP_HELPER_HPP
