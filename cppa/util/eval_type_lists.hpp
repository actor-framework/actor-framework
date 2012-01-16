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


#ifndef EVAL_TYPE_LISTS_HPP
#define EVAL_TYPE_LISTS_HPP

#include <type_traits>

#include "cppa/util/type_list.hpp"

namespace cppa { namespace util {

/**
 * @brief Apply @p What to each element of @p List.
 */
template <class ListA, class ListB,
          template <typename, typename> class What>
struct eval_type_lists
{

    typedef typename ListA::head head_a;
    typedef typename ListA::tail tail_a;

    typedef typename ListB::head head_b;
    typedef typename ListB::tail tail_b;

    static constexpr bool value =
               !std::is_same<head_a, void_type>::value
            && !std::is_same<head_b, void_type>::value
            && What<head_a, head_b>::value
            && eval_type_lists<tail_a, tail_b, What>::value;

};

template <template <typename, typename> class What>
struct eval_type_lists<type_list<>, type_list<>, What>
{
    static constexpr bool value = true;
};

} } // namespace cppa::util

#endif // EVAL_TYPE_LISTS_HPP
