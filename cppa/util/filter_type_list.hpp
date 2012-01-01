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


#ifndef FILTER_TYPE_LIST_HPP
#define FILTER_TYPE_LIST_HPP

#include "cppa/util/type_list.hpp"

namespace cppa { namespace util {

template<typename Needle, typename Haystack>
struct filter_type_list;

template<typename Needle, typename... Tail>
struct filter_type_list<Needle, util::type_list<Needle, Tail...>>
{
    typedef typename filter_type_list<Needle, util::type_list<Tail...>>::type type;
};

template<typename Needle, typename... Tail>
struct filter_type_list<Needle, util::type_list<Needle*, Tail...>>
{
    typedef typename filter_type_list<Needle, util::type_list<Tail...>>::type type;
};

template<typename Needle, typename Head, typename... Tail>
struct filter_type_list<Needle, util::type_list<Head, Tail...>>
{
    typedef typename concat_type_lists<util::type_list<Head>, typename filter_type_list<Needle, util::type_list<Tail...>>::type>::type type;
};

template<typename Needle>
struct filter_type_list<Needle, util::type_list<>>
{
    typedef util::type_list<> type;
};

} } // namespace cppa::util

#endif // FILTER_TYPE_LIST_HPP
