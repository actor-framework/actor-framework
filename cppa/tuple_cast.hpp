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
#include "cppa/cow_tuple.hpp"

#include "cppa/detail/matches.hpp"
#include "cppa/detail/type_list.hpp"

namespace cppa {

template<typename... T>
optional<typename detail::tl_apply<
    typename detail::tl_filter_not<detail::type_list<T...>, is_anything>::type,
    cow_tuple
>::type>
tuple_cast(message) {
    return none; //TODO: implement me
}

} // namespace cppa

#endif // CPPA_TUPLE_CAST_HPP
