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

#ifndef CPPA_DETAIL_TUPLE_ZIP_HPP
#define CPPA_DETAIL_TUPLE_ZIP_HPP

#include <tuple>

#include "cppa/detail/int_list.hpp"

namespace cppa {
namespace detail {

template<typename F, long... Is, class Tup0, class Tup1>
auto tuple_zip(F& f, detail::int_list<Is...>, Tup0&& tup0, Tup1&& tup1)
-> decltype(std::make_tuple(f(std::get<Is>(tup0), std::get<Is>(tup1))...)) {
    return std::make_tuple(f(std::get<Is>(tup0), std::get<Is>(tup1))...);
}

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_TUPLE_ZIP_HPP
