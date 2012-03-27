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
#include "cppa/partial_function.hpp"

#include "cppa/util/tbind.hpp"
#include "cppa/util/type_list.hpp"

#include "cppa/detail/invokable.hpp"


namespace cppa { namespace detail {

template<typename Statement>
struct receive_while_helper
{
    Statement m_stmt;

    template<typename S>
    receive_while_helper(S&& stmt) : m_stmt(std::forward<S>(stmt)) { }

    void operator()(behavior& bhvr)
    {
        local_actor* sptr = self;
        while (m_stmt()) sptr->dequeue(bhvr);
    }

    void operator()(partial_function& fun)
    {
        local_actor* sptr = self;
        while (m_stmt()) sptr->dequeue(fun);
    }

    void operator()(behavior&& bhvr)
    {
        behavior tmp{std::move(bhvr)};
        (*this)(tmp);
    }

    template<typename... Args>
    void operator()(partial_function&& arg0, Args&&... args)
    {
        typename select_bhvr<Args...>::type tmp;
        (*this)(tmp.splice(std::move(arg0), std::forward<Args>(args)...));
    }

};

template<typename T>
class receive_for_helper
{

    T& begin;
    T end;

 public:

    receive_for_helper(T& first, T const& last) : begin(first), end(last) { }

    void operator()(behavior& bhvr)
    {
        local_actor* sptr = self;
        for ( ; begin != end; ++begin) sptr->dequeue(bhvr);
    }

    void operator()(partial_function& fun)
    {
        local_actor* sptr = self;
        for ( ; begin != end; ++begin) sptr->dequeue(fun);
    }

    void operator()(behavior&& bhvr)
    {
        behavior tmp{std::move(bhvr)};
        (*this)(tmp);
    }

    template<typename... Args>
    void operator()(partial_function&& arg0, Args&&... args)
    {
        typename select_bhvr<Args...>::type tmp;
        (*this)(tmp.splice(std::move(arg0), std::forward<Args>(args)...));
    }

};

class do_receive_helper
{

    behavior m_bhvr;

 public:

    do_receive_helper(behavior&& bhvr) : m_bhvr(std::move(bhvr))
    {
    }

    template<typename Arg0, typename... Args>
    do_receive_helper(Arg0&& arg0, Args&&... args)
    {
        m_bhvr.splice(std::forward<Arg0>(arg0), std::forward<Args>(args)...);
    }

    do_receive_helper(do_receive_helper&&) = default;

    template<typename Statement>
    void until(Statement&& stmt)
    {
        static_assert(std::is_same<bool, decltype(stmt())>::value,
                      "functor or function does not return a boolean");
        local_actor* sptr = self;
        if (m_bhvr.timeout().valid())
        {
            do
            {
                sptr->dequeue(m_bhvr);
            }
            while (stmt() == false);
        }
        else
        {
            do
            {
                sptr->dequeue(m_bhvr.get_partial_function());
            }
            while (stmt() == false);
        }
    }

};

} } // namespace cppa::detail

#endif // RECEIVE_LOOP_HELPER_HPP
