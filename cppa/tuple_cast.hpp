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


#ifndef TUPLE_CAST_HPP
#define TUPLE_CAST_HPP

#include "cppa/pattern.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/util/option.hpp"
#include "cppa/detail/matches.hpp"

namespace cppa {

template<typename... P>
auto tuple_cast(any_tuple const& tup, pattern<P...> const& p)
    -> util::option<typename tuple_from_type_list<typename pattern<P...>::filtered_types>::type>
{
    typedef typename pattern<P...>::mapping_vector mapping_vector;
    typedef typename pattern<P...>::filtered_types filtered_types;
    typedef typename tuple_from_type_list<filtered_types>::type tuple_type;
    util::option<tuple_type> result;
    mapping_vector mv;
    if (detail::matches(detail::pm_decorated(tup.begin(), &mv), p.begin()))
    {
        if (mv.size() == tup.size()) // perfect match
        {
            result = tuple_type::from(tup.vals());
        }
        else
        {
            result = tuple_type::from(new detail::decorated_tuple<filtered_types::size>(tup.vals(), mv));
        }
    }
    return std::move(result);
}

}

#endif // TUPLE_CAST_HPP
