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


#ifndef CPPA_LEFT_OR_RIGHT_HPP
#define CPPA_LEFT_OR_RIGHT_HPP

#include "cppa/unit.hpp"

namespace cppa { namespace util {

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

} } // namespace cppa::util

#endif // CPPA_LEFT_OR_RIGHT_HPP
