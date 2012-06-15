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


#ifndef CPPA_TUPLE_CAST_HPP
#define CPPA_TUPLE_CAST_HPP

#include <type_traits>

#include "cppa/option.hpp"
#include "cppa/pattern.hpp"

#include "cppa/detail/matches.hpp"
#include "cppa/detail/tuple_cast_impl.hpp"

namespace cppa {

#ifdef CPPA_DOCUMENTATION

/**
 * @brief Tries to cast @p tup to {@link cow_tuple cow_tuple<T...>} and moves
 *        the content of @p tup to the returned tuple on success.
 * @param tup Dynamically typed tuple.
 * @param pttrn Requested types with optional guard values.
 * @returns An {@link option} for a {@link cow_tuple} with the types
 *          deduced from @p pttrn.
 * @relates any_tuple
 */
auto moving_tuple_cast(any_tuple& tup, const pattern<T...>& pttrn);

/**
 * @brief Tries to cast @p tup to {@link cow_tuple cow_tuple<T...>} and moves
 *        the content of @p tup to the returned tuple on success.
 * @param tup Dynamically typed tuple.
 * @returns An {@link option} for a {@link cow_tuple} with the types
 *          deduced from {T...}.
 * @relates any_tuple
 */
template<typename... T>
auto moving_tuple_cast(any_tuple& tup);

/**
 * @brief Tries to cast @p tup to {@link cow_tuple cow_tuple<T...>} and moves
 *        the content of @p tup to the returned tuple on success.
 * @param tup Dynamically typed tuple.
 * @returns An {@link option} for a {@link cow_tuple} with the types
 *          deduced from {T...}.
 * @relates any_tuple
 */
template<typename... T>
auto moving_tuple_cast(any_tuple& tup, const util::type_list<T...>&);

/**
 * @brief Tries to cast @p tup to {@link cow_tuple cow_tuple<T...>}.
 * @param tup Dynamically typed tuple.
 * @param pttrn Requested types with optional guard values.
 * @returns An {@link option} for a {@link cow_tuple} with the types
 *          deduced from @p pttrn.
 * @relates any_tuple
 */
template<typename... T>
auto tuple_cast(any_tuple tup, const pattern<T...>& pttrn);

/**
 * @brief Tries to cast @p tup to {@link cow_tuple cow_tuple<T...>}.
 * @param tup Dynamically typed tuple.
 * @returns An {@link option} for a {@link cow_tuple} with the types
 *          deduced from {T...}.
 * @relates any_tuple
 */
template<typename... T>
auto tuple_cast(any_tuple tup);

/**
 * @brief Tries to cast @p tup to {@link cow_tuple cow_tuple<T...>}.
 * @param tup Dynamically typed tuple.
 * @returns An {@link option} for a {@link cow_tuple} with the types
 *          deduced from {T...}.
 * @relates any_tuple
 */
template<typename... T>
auto tuple_cast(any_tuple tup, const util::type_list<T...>&);

#else

template<typename... T>
auto moving_tuple_cast(any_tuple& tup, const pattern<T...>& pttrn)
    -> option<
        typename cow_tuple_from_type_list<
            typename pattern<T...>::filtered_types
        >::type> {
    typedef typename pattern<T...>::filtered_types filtered_types;
    typedef typename cow_tuple_from_type_list<filtered_types>::type tuple_type;
    static constexpr auto impl = get_wildcard_position<util::type_list<T...>>();
    return detail::tuple_cast_impl<impl, tuple_type, T...>::safe(tup, pttrn);
}

template<typename... T>
auto moving_tuple_cast(any_tuple& tup)
    -> option<
        typename cow_tuple_from_type_list<
            typename util::tl_filter_not<util::type_list<T...>,
                                         is_anything>::type
        >::type> {
    typedef decltype(moving_tuple_cast<T...>(tup)) result_type;
    typedef typename result_type::type tuple_type;
    static constexpr auto impl =
            get_wildcard_position<util::type_list<T...>>();
    return detail::tuple_cast_impl<impl, tuple_type, T...>::safe(tup);
}

template<typename... T>
auto moving_tuple_cast(any_tuple& tup, const util::type_list<T...>&)
    -> decltype(moving_tuple_cast<T...>(tup)) {
    return moving_tuple_cast<T...>(tup);
}

template<typename... T>
auto tuple_cast(any_tuple tup, const pattern<T...>& pttrn)
     -> option<
          typename cow_tuple_from_type_list<
            typename pattern<T...>::filtered_types
        >::type> {
    return moving_tuple_cast(tup, pttrn);
}

template<typename... T>
auto tuple_cast(any_tuple tup)
     -> option<
          typename cow_tuple_from_type_list<
            typename util::tl_filter_not<util::type_list<T...>,
                                         is_anything>::type
        >::type> {
    return moving_tuple_cast<T...>(tup);
}

template<typename... T>
auto tuple_cast(any_tuple tup, const util::type_list<T...>&)
    -> decltype(tuple_cast<T...>(tup)) {
    return moving_tuple_cast<T...>(tup);
}

// ************************ for in-library use only! ************************ //

// (moving) cast using a pattern; does not perform type checking
template<typename... T>
auto unsafe_tuple_cast(any_tuple& tup, const pattern<T...>& p)
    -> option<
        typename cow_tuple_from_type_list<
            typename pattern<T...>::filtered_types
        >::type> {
    typedef typename pattern<T...>::filtered_types filtered_types;
    typedef typename cow_tuple_from_type_list<filtered_types>::type tuple_type;
    static constexpr auto impl =
            get_wildcard_position<util::type_list<T...>>();
    return detail::tuple_cast_impl<impl, tuple_type, T...>::unsafe(tup, p);
}

template<typename... T>
auto unsafe_tuple_cast(any_tuple& tup, const util::type_list<T...>&)
    -> decltype(tuple_cast<T...>(tup)) {
    return tuple_cast<T...>(tup);
}

// cast using a pattern; does neither perform type checking nor checks values
template<typename... T>
auto forced_tuple_cast(any_tuple& tup, const pattern<T...>& p)
    -> typename cow_tuple_from_type_list<
            typename pattern<T...>::filtered_types
       >::type {
    typedef typename pattern<T...>::filtered_types filtered_types;
    typedef typename cow_tuple_from_type_list<filtered_types>::type tuple_type;
    static constexpr auto impl =
            get_wildcard_position<util::type_list<T...>>();
    return detail::tuple_cast_impl<impl, tuple_type, T...>::force(tup, p);
}

#endif // CPPA_DOCUMENTATION

} // namespace cppa

#endif // CPPA_TUPLE_CAST_HPP
