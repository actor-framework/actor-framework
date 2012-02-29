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


#ifndef TUPLE_CAST_IMPL_HPP
#define TUPLE_CAST_IMPL_HPP

#include "cppa/match.hpp"
#include "cppa/pattern.hpp"
#include "cppa/type_value_pair.hpp"

#include "cppa/detail/tuple_vals.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/object_array.hpp"
#include "cppa/detail/abstract_tuple.hpp"
#include "cppa/detail/decorated_tuple.hpp"

namespace cppa { namespace detail {

enum class tuple_cast_impl_id
{
    no_wildcard,
    trailing_wildcard,
    leading_wildcard,
    wildcard_in_between
};

// covers wildcard_in_between and multiple_wildcards
template<wildcard_position, class Result, typename... T>
struct tuple_cast_impl
{
    static constexpr size_t size =
            util::tl_count_not<util::type_list<T...>, is_anything>::value;
    typedef util::fixed_vector<size_t, size> mapping_vector;
    template<class Tuple>
    inline static option<Result> _(Tuple const& tup)
    {
        mapping_vector mv;
        if (match<T...>(tup, mv)) return {Result::from(tup.vals(), mv)};
        return {};
    }
    template<class Tuple>
    inline static option<Result> _(Tuple const& tup, pattern<T...> const& p)
    {
        mapping_vector mv;
        if (match(tup, p)) return {Result::from(tup.vals(), mv)};
        return {};
    }
};

template<class Result, typename... T>
struct tuple_cast_impl<wildcard_position::nil, Result, T...>
{
    template<class Tuple>
    static inline option<Result> _(Tuple const& tup)
    {
        if (match<T...>(tup)) return {Result::from(tup.vals())};
        return {};
    }
     template<class Tuple>
     static inline option<Result> _(Tuple const& tup, pattern<T...> const& p)
     {
         if (match(tup, p)) return {Result::from(tup.vals())};
         return {};
     }
};

template<class Result, typename... T>
struct tuple_cast_impl<wildcard_position::trailing, Result, T...>
        : tuple_cast_impl<wildcard_position::nil, Result, T...>
{
};

template<class Result, typename... T>
struct tuple_cast_impl<wildcard_position::leading, Result, T...>
{
    template<class Tuple>
    inline static option<Result> _(Tuple const& tup)
    {
        size_t o = tup.size() - (sizeof...(T) - 1);
        if (match<T...>(tup)) return {Result::offset_subtuple(tup.vals(), o)};
        return {};
    }
    template<class Tuple>
    inline static option<Result> _(Tuple const& tup, pattern<T...> const& p)
    {
        size_t o = tup.size() - (sizeof...(T) - 1);
        if (match(tup, p)) return {Result::offset_subtuple(tup.vals(), o)};
        return {};
    }
};

} }

#endif // TUPLE_CAST_IMPL_HPP
