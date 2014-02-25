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
 * Free Software Foundation; either version 2.1 of the License,               *
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


#ifndef CPPA_GET_HPP
#define CPPA_GET_HPP

#include <tuple>
#include <cstddef>

#include "cppa/util/type_traits.hpp"

namespace cppa {

// forward declaration of details
namespace detail {
template<typename...> struct tdata;
template<typename...> struct pseudo_tuple;
} // namespace detail

namespace util {
template<typename...> struct type_list;
} // namespace util

// forward declaration of cow_tuple
template<typename...> class cow_tuple;

// forward declaration of get(const detail::tdata<...>&)
template<size_t N, typename... Ts>
const typename util::type_at<N, Ts...>::type& get(const detail::tdata<Ts...>&);

// forward declarations of get(const tuple<...>&)
template<size_t N, typename... Ts>
const typename util::type_at<N, Ts...>::type& get(const cow_tuple<Ts...>&);

// forward declarations of get(detail::pseudo_tuple<...>&)
template<size_t N, typename... Ts>
const typename util::type_at<N, Ts...>::type& get(const detail::pseudo_tuple<Ts...>& tv);

// forward declarations of get(util::type_list<...>&)
template<size_t N, typename... Ts>
typename util::type_at<N, Ts...>::type get(const util::type_list<Ts...>&);

// forward declarations of get_ref(detail::tdata<...>&)
template<size_t N, typename... Ts>
typename util::type_at<N, Ts...>::type& get_ref(detail::tdata<Ts...>&);

// forward declarations of get_ref(tuple<...>&)
template<size_t N, typename... Ts>
typename util::type_at<N, Ts...>::type& get_ref(cow_tuple<Ts...>&);

// forward declarations of get_ref(detail::pseudo_tuple<...>&)
template<size_t N, typename... Ts>
typename util::type_at<N, Ts...>::type& get_ref(detail::pseudo_tuple<Ts...>& tv);

// support get_ref access to std::tuple
template<size_t Pos, typename... Ts>
inline auto get_ref(std::tuple<Ts...>& tup) -> decltype(std::get<Pos>(tup)) {
    return std::get<Pos>(tup);
}

/**
 * @brief This function grants either const or non-const access to @p tup,
 *        depending on the cv-qualifier of @p tup.
 */
template<size_t Pos, class Tuple>
inline auto get_cv_aware(Tuple& tup) -> decltype(get_ref<Pos>(tup)) {
    return get_ref<Pos>(tup);
}

/**
 * @brief This function grants either const or non-const access to @p tup,
 *        depending on the cv-qualifier of @p tup.
 */
template<size_t Pos, class Tuple>
inline auto get_cv_aware(const Tuple& tup) -> decltype(get<Pos>(tup)) {
    return get<Pos>(tup);
}

} // namespace cppa

#endif // CPPA_GET_HPP
