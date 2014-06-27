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


#ifndef CPPA_TUPLE_CAST_HPP
#define CPPA_TUPLE_CAST_HPP

#include <type_traits>

#include "cppa/optional.hpp"

#include "cppa/util/type_list.hpp"

#include "cppa/detail/matches.hpp"
#include "cppa/detail/tuple_cast_impl.hpp"

namespace cppa {

#ifdef CPPA_DOCUMENTATION

/**
 * @brief Tries to cast @p tup to {@link cow_tuple cow_tuple<T...>} and moves
 *        the content of @p tup to the returned tuple on success.
 * @param tup Dynamically typed tuple.
 * @returns An {@link option} for a {@link cow_tuple} with the types
 *          deduced from {T...}.
 * @relates message
 */
template<typename... T>
auto moving_tuple_cast(message& tup);

/**
 * @brief Tries to cast @p tup to {@link cow_tuple cow_tuple<T...>} and moves
 *        the content of @p tup to the returned tuple on success.
 * @param tup Dynamically typed tuple.
 * @returns An {@link option} for a {@link cow_tuple} with the types
 *          deduced from {T...}.
 * @relates message
 */
template<typename... T>
auto moving_tuple_cast(message& tup, const util::type_list<T...>&);

/**
 * @brief Tries to cast @p tup to {@link cow_tuple cow_tuple<T...>}.
 * @param tup Dynamically typed tuple.
 * @returns An {@link option} for a {@link cow_tuple} with the types
 *          deduced from {T...}.
 * @relates message
 */
template<typename... T>
auto tuple_cast(message tup);

/**
 * @brief Tries to cast @p tup to {@link cow_tuple cow_tuple<T...>}.
 * @param tup Dynamically typed tuple.
 * @returns An {@link option} for a {@link cow_tuple} with the types
 *          deduced from {T...}.
 * @relates message
 */
template<typename... T>
auto tuple_cast(message tup, const util::type_list<T...>&);

#else

template<typename... T>
auto moving_tuple_cast(message& tup)
    -> optional<
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
auto moving_tuple_cast(message& tup, const util::type_list<T...>&)
    -> decltype(moving_tuple_cast<T...>(tup)) {
    return moving_tuple_cast<T...>(tup);
}

template<typename... T>
auto tuple_cast(message tup)
     -> optional<
          typename cow_tuple_from_type_list<
            typename util::tl_filter_not<util::type_list<T...>,
                                         is_anything>::type
        >::type> {
    return moving_tuple_cast<T...>(tup);
}

template<typename... T>
auto tuple_cast(message tup, const util::type_list<T...>&)
    -> decltype(tuple_cast<T...>(tup)) {
    return moving_tuple_cast<T...>(tup);
}

// ************************ for in-library use only! ************************ //

template<typename... T>
auto unsafe_tuple_cast(message& tup, const util::type_list<T...>&)
    -> decltype(tuple_cast<T...>(tup)) {
    return tuple_cast<T...>(tup);
}

#endif // CPPA_DOCUMENTATION

} // namespace cppa

#endif // CPPA_TUPLE_CAST_HPP
