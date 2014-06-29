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

#ifndef CPPA_TYPE_PAIR_HPP
#define CPPA_TYPE_PAIR_HPP

namespace cppa {
namespace detail {

/**
 * @ingroup MetaProgramming
 * @brief A pair of two types.
 */
template<typename First, typename Second>
struct type_pair {
    typedef First first;
    typedef Second second;

};

template<typename First, typename Second>
struct to_type_pair {
    typedef type_pair<First, Second> type;

};

template<class What>
struct is_type_pair {
    static constexpr bool value = false;

};

template<typename First, typename Second>
struct is_type_pair<type_pair<First, Second>> {
    static constexpr bool value = true;

};

} // namespace detail
} // namespace cppa

#endif // CPPA_TYPE_PAIR_HPP
