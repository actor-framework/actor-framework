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

#include <type_traits>

#include "cppa/option.hpp"
#include "cppa/pattern.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/detail/matches.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/decorated_tuple.hpp"
#include "cppa/detail/implicit_conversions.hpp"

namespace cppa {

// cast using a pattern
template<class ResultTuple, class Tuple, typename... P>
option<ResultTuple> tuple_cast_impl(Tuple const& tup, pattern<P...> const& p)
{
    typedef util::type_list<P...> types;
    static constexpr int apos = util::tl_find<types, anything>::value;
    // no anything in given template parameter pack
    // or anything at end of template parameter pack
    if (apos == -1 || apos == (sizeof...(P) - 1))
    {
        if (detail::matches(tup, p))
        {
            return {ResultTuple::from(tup.vals())};
        }
    }
    else
    {
        typename pattern<P...>::mapping_vector mv;
        if (detail::matches(tup, p, &mv))
        {
            if (mv.size() == tup.size()) // perfect match
            {
                return {ResultTuple::from(tup.vals())};
            }
            else
            {
                return {ResultTuple::from(tup.vals(), mv)};
            }
        }
    }
    return { };
}

// cast using types
template<class ResultTuple, class Tuple, typename... T>
option<ResultTuple> tuple_cast_impl(Tuple const& tup)
{
    typedef util::type_list<T...> types;
    static constexpr int apos = util::tl_find<types, anything>::value;
    // no anything in given template parameter pack
    // or anything at end of template parameter pack
    if (apos == -1 || apos == (sizeof...(T) - 1))
    {
        if (tup.size() >= sizeof...(T))
        {
            auto& tarr = detail::static_types_array<T...>::arr;
            static constexpr int end = (apos == -1) ? sizeof...(T) : apos;
            for (int i = 0; i < end; ++i)
            {
                if (tarr[i] != tup.type_at(i)) return { };
            }
            // always a perfect match or subtuple
            return {ResultTuple::from(tup.vals())};
        }
    }
    else
    {
        util::fixed_vector<size_t, ResultTuple::num_elements> mv;
        if (detail::matches(tup, detail::static_types_array<T...>::arr, &mv))
        {
            // never a perfect match
            return {ResultTuple::from(tup.vals(), mv)};
        }
    }
    return { };
}

// cast using a pattern
template<typename... P>
auto tuple_cast(any_tuple const& tup, pattern<P...> const& p)
    -> option<
        typename tuple_from_type_list<
            typename pattern<P...>::filtered_types
        >::type>
{
    typedef typename pattern<P...>::filtered_types filtered_types;
    typedef typename tuple_from_type_list<filtered_types>::type tuple_type;
    return tuple_cast_impl<tuple_type, any_tuple, P...>(tup, p);
}

// cast using types
template<typename... T>
auto tuple_cast(any_tuple const& tup)
    -> option<
        typename tuple_from_type_list<
            typename util::tl_filter_not<
                util::type_list<T...>,
                util::tbind<std::is_same, anything>::type
            >::type
        >::type>
{
    typedef decltype(tuple_cast<T...>(tup)) result_type;
    typedef typename result_type::value_type tuple_type;
    return tuple_cast_impl<tuple_type, any_tuple, T...>(tup);
}

/*
template<typename... P>
auto tuple_cast(any_tuple_view const& tup, pattern<P...> const& p)
    -> option<
        typename tuple_view_from_type_list<
            typename pattern<P...>::filtered_types
        >::type>
{
    typedef typename pattern<P...>::filtered_types filtered_types;
    typedef typename tuple_view_from_type_list<filtered_types>::type tuple_type;
    return tuple_cast_impl<tuple_type, any_tuple_view, P...>(tup, p);
}

// cast using types
template<typename... T>
auto tuple_cast(any_tuple_view const& tup)
    -> option<
        typename tuple_view_from_type_list<
            typename util::tl_filter_not<
                util::type_list<T...>,
                util::tbind<std::is_same, anything>::type
            >::type
        >::type>
{
    typedef decltype(tuple_cast<T...>(tup)) result_type;
    typedef typename result_type::value_type tuple_type;
    return tuple_cast_impl<tuple_type, any_tuple_view, T...>(tup);
}
*/

} // namespace cppa

#endif // TUPLE_CAST_HPP
