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


#ifndef CPPA_COMPARE_TUPLES_HPP
#define CPPA_COMPARE_TUPLES_HPP

#include "cppa/get.hpp"
#include "cppa/util/at.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/is_comparable.hpp"

namespace cppa { namespace detail {

template<size_t N, template<typename...> class Tuple, typename... Types>
const typename util::at<N, Types...>::type&
do_get(const Tuple<Types...>& t) {
    return ::cppa::get<N, Types...>(t);
}

template<size_t N, typename LhsTuple, typename RhsTuple>
struct cmp_helper {
    inline static bool cmp(const LhsTuple& lhs, const RhsTuple& rhs) {
        return    do_get<N>(lhs) == do_get<N>(rhs)
               && cmp_helper<N-1, LhsTuple, RhsTuple>::cmp(lhs, rhs);
    }
};

template<typename LhsTuple, typename RhsTuple>
struct cmp_helper<0, LhsTuple, RhsTuple> {
    inline static bool cmp(const LhsTuple& lhs, const RhsTuple& rhs) {
        return do_get<0>(lhs) == do_get<0>(rhs);
    }
};

} } // namespace cppa::detail

namespace cppa { namespace util {

template<template<typename...> class LhsTuple, typename... LhsTypes,
         template<typename...> class RhsTuple, typename... RhsTypes>
bool compare_tuples(const LhsTuple<LhsTypes...>& lhs,
                    const RhsTuple<RhsTypes...>& rhs) {
    static_assert(sizeof...(LhsTypes) == sizeof...(RhsTypes),
                  "could not compare tuples of different size");

    static_assert(tl_binary_forall<
                      type_list<LhsTypes...>,
                      type_list<RhsTypes...>,
                      is_comparable
                  >::value,
                  "types of lhs are not comparable to the types of rhs");

    return detail::cmp_helper<(sizeof...(LhsTypes) - 1),
                              LhsTuple<LhsTypes...>,
                              RhsTuple<RhsTypes...>>::cmp(lhs, rhs);
}

template<template<typename...> class LhsTuple, typename... LhsTypes,
         template<typename...> class RhsTuple, typename... RhsTypes>
bool compare_first_elements(const LhsTuple<LhsTypes...>& lhs,
                            const RhsTuple<RhsTypes...>& rhs) {
    typedef typename tl_zip<
                util::type_list<LhsTypes...>,
                util::type_list<RhsTypes...>
            >::type
            zipped_types;

    static_assert(tl_zipped_forall<zipped_types, is_comparable>::value,
                  "types of lhs are not comparable to the types of rhs");

    return detail::cmp_helper<(zipped_types::size - 1),
                              LhsTuple<LhsTypes...>,
                              RhsTuple<RhsTypes...>>::cmp(lhs, rhs);
}

} } // namespace cppa::util

#endif // CPPA_COMPARE_TUPLES_HPP
