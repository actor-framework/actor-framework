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


#ifndef CPPA_GET_HPP
#define CPPA_GET_HPP

#include <tuple>
#include <cstddef>

#include "cppa/util/type_traits.hpp"
#include "cppa/util/rebindable_reference.hpp"

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

// forward declarations of get(detail::type_list<...>&)
template<size_t N, typename... Ts>
typename util::type_at<N, Ts...>::type get(const detail::type_list<Ts...>&);

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
inline auto get_cv_aware(Tuple& tup)
-> decltype(util::unwrap_ref(get_ref<Pos>(tup))) {
    return util::unwrap_ref(get_ref<Pos>(tup));
}

/**
 * @brief This function grants either const or non-const access to @p tup,
 *        depending on the cv-qualifier of @p tup.
 */
template<size_t Pos, class Tuple>
inline auto get_cv_aware(const Tuple& tup)
-> decltype(util::unwrap_ref(get<Pos>(tup))) {
    return util::unwrap_ref(get<Pos>(tup));
}

} // namespace cppa

#endif // CPPA_GET_HPP
