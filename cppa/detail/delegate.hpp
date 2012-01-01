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


#ifndef DELEGATE_HPP
#define DELEGATE_HPP

namespace cppa { namespace detail {

class delegate
{

    typedef void (*void_fun)(void*, void*);

    void_fun m_fun;
    void* m_arg1;
    void* m_arg2;

 public:

    template<typename Arg1, typename Arg2, typename Function>
    delegate(Function* fun, Arg1* a1, Arg2* a2)
        : m_fun(reinterpret_cast<void_fun>(fun))
        , m_arg1(reinterpret_cast<void*>(a1))
        , m_arg2(reinterpret_cast<void*>(a2))
    {
    }

    template<typename Arg1, typename Arg2, typename Function>
    void reset(Function* fun, Arg1* a1, Arg2* a2)
    {
        m_fun = reinterpret_cast<void_fun>(fun);
        m_arg1 = reinterpret_cast<void*>(a1);
        m_arg2 = reinterpret_cast<void*>(a2);
    }

    void operator()();

};

} } // namespace cppa::detail

#endif // DELEGATE_HPP
