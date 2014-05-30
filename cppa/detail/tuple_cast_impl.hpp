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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#ifndef CPPA_DETAIL_TUPLE_CAST_IMPL_HPP
#define CPPA_DETAIL_TUPLE_CAST_IMPL_HPP

#include "cppa/any_tuple.hpp"

#include "cppa/detail/matches.hpp"
#include "cppa/detail/tuple_vals.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/abstract_tuple.hpp"

namespace cppa {
namespace detail {

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
    typedef util::limited_vector<size_t, size> mapping_vector;
    static inline optional<Result> safe(any_tuple& tup) {
        mapping_vector mv;
        if (matches<T...>(tup, mv)) return {Result::from(std::move(tup.vals()),
                                                         mv)};
        return none;
    }
};

template<class Result, typename... T>
struct tuple_cast_impl<wildcard_position::nil, Result, T...> {
    static inline optional<Result> safe(any_tuple& tup) {
        if (matches<T...>(tup)) return {Result::from(std::move(tup.vals()))};
        return none;
    }
};

template<class Result, typename... T>
struct tuple_cast_impl<wildcard_position::trailing, Result, T...>
        : tuple_cast_impl<wildcard_position::nil, Result, T...> {
};

template<class Result, typename... T>
struct tuple_cast_impl<wildcard_position::leading, Result, T...> {
    static inline optional<Result> safe(any_tuple& tup) {
        size_t o = tup.size() - (sizeof...(T) - 1);
        if (matches<T...>(tup)) return {Result::offset_subtuple(tup.vals(), o)};
        return none;
    }
};

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_TUPLE_CAST_IMPL_HPP
