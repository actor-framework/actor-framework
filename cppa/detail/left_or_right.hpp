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

#ifndef CPPA_LEFT_OR_RIGHT_HPP
#define CPPA_LEFT_OR_RIGHT_HPP

#include "cppa/unit.hpp"

namespace cppa {
namespace detail {

/**
 * @brief Evaluates to @p Right if @p Left == unit_t, @p Left otherwise.
 */
template<typename Left, typename Right>
struct left_or_right {
    typedef Left type;

};

template<typename Right>
struct left_or_right<unit_t, Right> {
    typedef Right type;

};

template<typename Right>
struct left_or_right<unit_t&, Right> {
    typedef Right type;

};

template<typename Right>
struct left_or_right<const unit_t&, Right> {
    typedef Right type;

};

/**
 * @brief Evaluates to @p Right if @p Left != unit_t, @p unit_t otherwise.
 */
template<typename Left, typename Right>
struct if_not_left {
    typedef unit_t type;

};

template<typename Right>
struct if_not_left<unit_t, Right> {
    typedef Right type;

};

} // namespace detail
} // namespace cppa

#endif // CPPA_LEFT_OR_RIGHT_HPP
