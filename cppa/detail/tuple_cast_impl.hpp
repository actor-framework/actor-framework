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
 * Copyright (C) 2011-2013                                                    *
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


#ifndef CPPA_TUPLE_CAST_IMPL_HPP
#define CPPA_TUPLE_CAST_IMPL_HPP

#include "cppa/any_tuple.hpp"

#include "cppa/detail/matches.hpp"
#include "cppa/detail/tuple_vals.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/object_array.hpp"
#include "cppa/detail/abstract_tuple.hpp"
#include "cppa/detail/decorated_tuple.hpp"

namespace cppa { namespace detail {

enum class tuple_cast_impl_id {
    no_wildcard,
    trailing_wildcard,
    leading_wildcard,
    wildcard_in_between
};

// covers wildcard_in_between and multiple_wildcards
template<wildcard_position WP, class Result, typename... T>
struct tuple_cast_impl {
    static constexpr size_t size =
            util::tl_count_not<util::type_list<T...>, is_anything>::value;
    static constexpr size_t first_wc =
            static_cast<size_t>(
                util::tl_find<util::type_list<T...>, anything>::value);
    typedef util::fixed_vector<size_t, size> mapping_vector;
    static inline option<Result> safe(any_tuple& tup) {
        mapping_vector mv;
        if (matches<T...>(tup, mv)) return {Result::from(std::move(tup.vals()),
                                                         mv)};
        return {};
    }
};

template<class Result, typename... T>
struct tuple_cast_impl<wildcard_position::nil, Result, T...> {
    static inline option<Result> safe(any_tuple& tup) {
        if (matches<T...>(tup)) return {Result::from(std::move(tup.vals()))};
        return {};
    }
};

template<class Result, typename... T>
struct tuple_cast_impl<wildcard_position::trailing, Result, T...>
        : tuple_cast_impl<wildcard_position::nil, Result, T...> {
};

template<class Result, typename... T>
struct tuple_cast_impl<wildcard_position::leading, Result, T...> {
    static inline option<Result> safe(any_tuple& tup) {
        size_t o = tup.size() - (sizeof...(T) - 1);
        if (matches<T...>(tup)) return {Result::offset_subtuple(tup.vals(), o)};
        return {};
    }
};

} }

#endif // CPPA_TUPLE_CAST_IMPL_HPP
