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


#ifndef EVAL_FIRST_N_HPP
#define EVAL_FIRST_N_HPP

#include <type_traits>

#include "cppa/util/first_n.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/eval_type_lists.hpp"

namespace cppa { namespace util {

template <size_t N,
          class ListA, class ListB,
          template <typename, typename> class What>
struct eval_first_n;

template <size_t N, typename... TypesA, typename... TypesB,
          template <typename, typename> class What>
struct eval_first_n<N, type_list<TypesA...>, type_list<TypesB...>, What>
{
    typedef type_list<TypesA...> first_list;
    typedef type_list<TypesB...> second_list;

    typedef typename first_n<N, first_list>::type slist_a;
    typedef typename first_n<N, second_list>::type slist_b;

    static constexpr bool value = eval_type_lists<slist_a, slist_b, What>::value;
};

} } // namespace cppa::util

#endif // EVAL_FIRST_N_HPP
