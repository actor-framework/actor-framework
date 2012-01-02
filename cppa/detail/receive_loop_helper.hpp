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


#ifndef RECEIVE_LOOP_HELPER_HPP
#define RECEIVE_LOOP_HELPER_HPP

#include <new>

#include "cppa/self.hpp"
#include "cppa/behavior.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/invoke_rules.hpp"


namespace cppa { namespace detail {

template<typename Statement>
struct receive_while_helper
{
    Statement m_stmt;

    template<typename S>
    receive_while_helper(S&& stmt) : m_stmt(std::forward<S>(stmt))
    {
    }

    void operator()(invoke_rules& rules)
    {
        local_actor* sptr = self;
        while (m_stmt())
        {
            sptr->dequeue(rules);
        }
    }

    void operator()(invoke_rules&& rules)
    {
        invoke_rules tmp(std::move(rules));
        (*this)(tmp);
    }

    void operator()(timed_invoke_rules& rules)
    {
        local_actor* sptr = self;
        while (m_stmt())
        {
            sptr->dequeue(rules);
        }
    }

    void operator()(timed_invoke_rules&& rules)
    {
        timed_invoke_rules tmp(std::move(rules));
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

    behavior m_bhvr;

    inline void init(timed_invoke_rules&& bhvr)
    {
        m_bhvr = std::move(bhvr);
    }

    inline void init(invoke_rules& bhvr)
    {
        m_bhvr = std::move(bhvr);
    }

    template<typename Arg0, typename... Args>
    inline void init(invoke_rules& rules, Arg0&& arg0, Args&&... args)
    {
        init(rules.splice(std::forward<Arg0>(arg0)),
             std::forward<Args>(args)...);
    }

 public:

    do_receive_helper(invoke_rules&& rules) : m_bhvr(std::move(rules))
    {
    }

    do_receive_helper(timed_invoke_rules&& rules) : m_bhvr(std::move(rules))
    {
    }

    template<typename Arg0, typename... Args>
    do_receive_helper(invoke_rules&& rules, Arg0&& arg0, Args&&... args)
    {
        invoke_rules tmp(std::move(rules));
        init(tmp.splice(std::forward<Arg0>(arg0)), std::forward<Args>(args)...);
    }

    do_receive_helper(do_receive_helper&& other)
        : m_bhvr(std::move(other.m_bhvr))
    {
    }

    template<typename Statement>
    void until(Statement&& stmt)
    {
        static_assert(std::is_same<bool, decltype(stmt())>::value,
                      "functor or function does not return a boolean");
        local_actor* sptr = self;
        if (m_bhvr.is_left())
        {
            do
            {
                sptr->dequeue(m_bhvr.left());
            }
            while (stmt() == false);
        }
        else
        {
            do
            {
                sptr->dequeue(m_bhvr.right());
            }
            while (stmt() == false);
        }
    }

};

} } // namespace cppa::detail

#endif // RECEIVE_LOOP_HELPER_HPP
