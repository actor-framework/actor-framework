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


#ifndef BEHAVIOR_HPP
#define BEHAVIOR_HPP

#include <functional>
#include <type_traits>

#include "cppa/util/tbind.hpp"
#include "cppa/util/rm_ref.hpp"
#include "cppa/util/if_else.hpp"
#include "cppa/util/duration.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/partial_function.hpp"

namespace cppa {

/**
 * @brief
 */
class behavior
{

    behavior(behavior const&) = delete;
    behavior& operator=(behavior const&) = delete;

 public:

    behavior() = default;
    behavior(behavior&&) = default;
    behavior& operator=(behavior&&) = default;


    inline behavior(partial_function&& fun) : m_fun(std::move(fun))
    {
    }

    inline behavior(util::duration tout, std::function<void()>&& handler)
        : m_timeout(tout), m_timeout_handler(std::move(handler))
    {
    }

    inline void handle_timeout() const
    {
        m_timeout_handler();
    }

    inline util::duration const& timeout() const
    {
        return m_timeout;
    }

    inline void operator()(any_tuple const& value)
    {
        m_fun(value);
    }

    inline detail::intermediate* get_intermediate(any_tuple const& value)
    {
        return m_fun.get_intermediate(value);
    }

    inline partial_function& get_partial_function()
    {
        return m_fun;
    }

    inline behavior& splice(behavior&& what)
    {
        m_fun.splice(std::move(what.get_partial_function()));
        m_timeout = what.m_timeout;
        m_timeout_handler = std::move(what.m_timeout_handler);
        return *this;
    }

    template<class... Args>
    inline behavior& splice(partial_function&& arg0, Args&&... args)
    {
        m_fun.splice(std::move(arg0));
        return splice(std::forward<Args>(args)...);
    }

 private:

    // terminates recursion
    inline behavior& splice() { return *this; }

    partial_function m_fun;
    util::duration m_timeout;
    std::function<void()> m_timeout_handler;

};

namespace detail {

template<typename... Ts>
struct select_bhvr
{
    static constexpr bool timed =
            util::tl_exists<
                util::type_list<Ts...>,
                util::tbind<std::is_same, behavior>::type
            >::value;
    typedef typename util::if_else_c<timed,
                                     behavior,
                                     util::wrapped<partial_function> >::type
            type;
};

} // namespace detail

} // namespace cppa

#endif // BEHAVIOR_HPP
