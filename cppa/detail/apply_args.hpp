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

#ifndef CALL_HPP
#define CALL_HPP

#include "cppa/util/int_list.hpp"

namespace cppa {
namespace detail {

template <typename F, long... Is, class Tuple>
inline auto apply_args(F& f, util::int_list<Is...>, Tuple&& tup)
    -> decltype(f(get<Is>(tup)...)) {
    return f(get<Is>(tup)...);
}

template <typename F, class Tuple, typename... Ts>
inline auto apply_args_prefixed(F& f, util::int_list<>, Tuple&, Ts&&... args)
    -> decltype(f(std::forward<Ts>(args)...)) {
    return f(std::forward<Ts>(args)...);
}

template <typename F, long... Is, class Tuple, typename... Ts>
inline auto apply_args_prefixed(F& f, util::int_list<Is...>, Tuple& tup,
                                Ts&&... args)
    -> decltype(f(std::forward<Ts>(args)..., get<Is>(tup)...)) {
    return f(std::forward<Ts>(args)..., get<Is>(tup)...);
}

template <typename F, long... Is, class Tuple, typename... Ts>
inline auto apply_args_suffxied(F& f, util::int_list<Is...>, Tuple& tup,
                                Ts&&... args)
    -> decltype(f(get<Is>(tup)..., std::forward<Ts>(args)...)) {
    return f(get<Is>(tup)..., std::forward<Ts>(args)...);
}

} // namespace detail
} // namespace cppa

#endif // CALL_HPP
