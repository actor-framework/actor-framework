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

#include "cppa/match_expr.hpp"
#include "cppa/partial_function.hpp"

#include "cppa/util/tbind.hpp"
#include "cppa/util/rm_ref.hpp"
#include "cppa/util/if_else.hpp"
#include "cppa/util/duration.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/disjunction.hpp"

namespace cppa {

/**
 * @brief Describes the behavior of an actor.
 */
class behavior
{

    friend behavior operator,(partial_function&& lhs, behavior&& rhs);

    behavior(behavior const&) = delete;
    behavior& operator=(behavior const&) = delete;

 public:

    behavior() = default;
    behavior(behavior&&) = default;

    inline behavior(partial_function&& fun) : m_fun(std::move(fun))
    {
    }

    template<typename... Cases>
    behavior(match_expr<Cases...> const& me) : m_fun(me) { }

    inline behavior(util::duration tout, std::function<void()>&& handler)
        : m_timeout(tout), m_timeout_handler(std::move(handler))
    {
    }

    behavior& operator=(behavior&&) = default;

    //behavior& operator=(partial_function&& pfun)
    //{
    //    m_fun = std::move(pfun);
    //    return *this;
    //}

    inline void handle_timeout() const
    {
        m_timeout_handler();
    }

    inline util::duration const& timeout() const
    {
        return m_timeout;
    }

    inline partial_function& get_partial_function()
    {
        return m_fun;
    }

 private:

    // terminates recursion
    inline behavior& splice() { return *this; }

    partial_function m_fun;
    util::duration m_timeout;
    std::function<void()> m_timeout_handler;

};

template<typename... Lhs>
behavior operator,(match_expr<Lhs...> const& lhs,
                   behavior&& rhs)
{
    rhs.get_partial_function() = lhs;
    return std::move(rhs);
}

template<typename Arg0>
behavior bhvr_collapse(Arg0&& arg)
{
    return {std::forward<Arg0>(arg)};
}

template<typename Arg0, typename Arg1, typename... Args>
behavior bhvr_collapse(Arg0&& arg0, Arg1&& arg1, Args&&... args)
{
    return bhvr_collapse((std::forward<Arg0>(arg0), std::forward<Arg1>(arg1)),
                         std::forward<Args>(args)...);
}

template<typename... Args>
typename std::enable_if<
    util::disjunction<std::is_same<behavior, Args>...>::value,
    behavior
>::type
match_expr_concat(Args&&... args)
{
    return bhvr_collapse(std::forward<Args>(args)...);
}

template<typename... Args>
typename std::enable_if<
    util::disjunction<
        std::is_same<
            behavior,
            typename util::rm_ref<Args>::type
        >...
    >::value == false,
    partial_function
>::type
match_expr_concat(Args&&... args)
{
    return mexpr_concat_convert(std::forward<Args>(args)...);
}

inline partial_function match_expr_concat(partial_function&& pfun)
{
    return std::move(pfun);
}

inline behavior match_expr_concat(behavior&& bhvr)
{
    return std::move(bhvr);
}

namespace detail {

template<typename... Ts>
struct select_bhvr
{
    static constexpr bool timed =
            util::disjunction<std::is_same<behavior, Ts>...>::value;
    typedef typename util::if_else_c<timed,
                                     behavior,
                                     util::wrapped<partial_function> >::type
            type;
};

} // namespace detail

} // namespace cppa

#endif // BEHAVIOR_HPP
