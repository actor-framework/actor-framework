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
 * Copyright (C) 2011-2014                                                    *
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


#ifndef CPPA_UTIL_CALL_HPP
#define CPPA_UTIL_CALL_HPP

#include "cppa/get.hpp"
#include "cppa/util/int_list.hpp"

namespace cppa {
namespace util {

template<typename F, class Tuple, long... Is>
inline auto apply_args(F& f, Tuple& tup, util::int_list<Is...>)
-> decltype(f(get_cv_aware<Is>(tup)...)) {
    return f(get_cv_aware<Is>(tup)...);
}

template<typename F, class Tuple, long... Is, typename... Ts>
inline auto apply_args_prefixed(F& f, Tuple& tup, util::int_list<Is...>, Ts&&... args)
-> decltype(f(std::forward<Ts>(args)..., get_cv_aware<Is>(tup)...)) {
    return f(std::forward<Ts>(args)..., get_cv_aware<Is>(tup)...);
}

template<typename F, class Tuple, long... Is, typename... Ts>
inline auto apply_args_suffxied(F& f, Tuple& tup, util::int_list<Is...>, Ts&&... args)
-> decltype(f(get_cv_aware<Is>(tup)..., std::forward<Ts>(args)...)) {
    return f(get_cv_aware<Is>(tup)..., std::forward<Ts>(args)...);
}

template<typename F, typename... Ts>
inline auto call_mv(F& f, Ts&&... args) -> decltype(f(std::move(args)...)) {
    return f(std::move(args)...);
}

} // namespace util
} // namespace cppa

#endif // CPPA_UTIL_CALL_HPP
