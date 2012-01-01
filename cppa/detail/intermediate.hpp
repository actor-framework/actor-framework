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


#ifndef INTERMEDIATE_HPP
#define INTERMEDIATE_HPP

#include <utility>

namespace cppa { namespace detail {

class intermediate
{

    intermediate(intermediate const&) = delete;
    intermediate& operator=(intermediate const&) = delete;

 public:

    intermediate() = default;

    virtual ~intermediate();

    virtual void invoke() = 0;

};

template<typename Impl, typename View = void>
class intermediate_impl : public intermediate
{

    Impl m_impl;
    View m_view;

 public:

    template<typename Arg0, typename Arg1>
    intermediate_impl(Arg0&& impl, Arg1&& view)
        : intermediate()
        , m_impl(std::forward<Arg0>(impl))
        , m_view(std::forward<Arg1>(view))
    {
    }

    virtual void invoke() /*override*/
    {
        m_impl(m_view);
    }

};

template<typename Impl>
class intermediate_impl<Impl, void> : public intermediate
{

    Impl m_impl;

 public:

    intermediate_impl(Impl const& impl) : m_impl(impl) { }

    intermediate_impl(Impl&& impl) : m_impl(std::move(impl)) { }

    virtual void invoke() /*override*/
    {
        m_impl();
    }

};

} } // namespace cppa::detail

#endif // INTERMEDIATE_HPP
