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


#ifndef MATCH_HPP
#define MATCH_HPP

#include <type_traits>

#include "cppa/tuple.hpp"
#include "cppa/invoke_rules.hpp"

#include "cppa/util/if_else.hpp"
#include "cppa/util/enable_if.hpp"

#include "cppa/detail/implicit_conversions.hpp"

namespace cppa {

namespace detail {

template<typename Tuple>
struct match_helper
{
    //static constexpr bool is_view = std::is_same<Tuple, any_tuple_view>::value;
    static constexpr bool is_view = false;
    Tuple what;
    template<typename T>
    match_helper(T const& w, typename util::enable_if_c<std::is_same<T, T>::value && is_view>::type* =0)
        : what(w) { }

    //template<typename T>
    //match_helper(T const& w, typename util::enable_if_c<std::is_same<T, T>::value && !is_view>::type* =0)
    //    : what(make_tuple(w)) { }
    void operator()(invoke_rules& rules) { rules(what); }
    void operator()(invoke_rules&& rules) { rules(what); }
    template<typename Head, typename... Tail>
    void operator()(invoke_rules&& bhvr, Head&& head, Tail&&... tail)
    {
        invoke_rules tmp(std::move(bhvr));
        (*this)(tmp.splice(std::forward<Head>(head)),
                std::forward<Tail>(tail)...);
    }
    template<typename Head, typename... Tail>
    void operator()(invoke_rules& bhvr, Head&& head, Tail&&... tail)
    {
        (*this)(bhvr.splice(std::forward<Head>(head)),
                std::forward<Tail>(tail)...);
    }
};

} // namespace detail

/*
template<typename T>
auto match(T const& x)
    -> detail::match_helper<
        typename util::if_else<
            std::is_same<typename detail::implicit_conversions<T>::type, T>,
            any_tuple_view,
            util::wrapped<any_tuple> >::type>
{
    return {x};
}
*/

} // namespace cppa

#endif // MATCH_HPP
