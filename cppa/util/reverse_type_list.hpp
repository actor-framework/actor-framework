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


#ifndef CPPA_UTIL_REVERSE_TYPE_LIST_HPP
#define CPPA_UTIL_REVERSE_TYPE_LIST_HPP

#include "cppa/util/type_list.hpp"
#include "cppa/util/concat_type_lists.hpp"

namespace cppa { namespace util {

template<typename ListFrom, typename ListTo = type_list<>>
struct reverse_type_list;

template<typename HeadA, typename... TailA, typename... ListB>
struct reverse_type_list<type_list<HeadA, TailA...>, type_list<ListB...>>
{
    typedef typename reverse_type_list<type_list<TailA...>,
                                       type_list<HeadA, ListB...>>::type
            type;
};

template<typename... Types>
struct reverse_type_list<type_list<>, type_list<Types...>>
{
    typedef type_list<Types...> type;
};

} } // namespace cppa::util

#endif // CPPA_UTIL_REVERSE_TYPE_LIST_HPP
