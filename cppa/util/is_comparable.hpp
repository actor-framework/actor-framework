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


#ifndef CPPA_IS_COMPARABLE_HPP
#define CPPA_IS_COMPARABLE_HPP

#include <type_traits>

namespace cppa { namespace util {

template<typename T1, typename T2>
class is_comparable {

    // SFINAE: If you pass a "bool*" as third argument, then
    //         decltype(cmp_help_fun(...)) is bool if there's an
    //         operator==(A,B) because
    //         cmp_help_fun(A*, B*, bool*) is a better match than
    //         cmp_help_fun(A*, B*, void*). If there's no operator==(A,B)
    //         available, then cmp_help_fun(A*, B*, void*) is the only
    //         candidate and thus decltype(cmp_help_fun(...)) is void.

    template<typename A, typename B>
    static bool cmp_help_fun(const A* arg0, const B* arg1,
                             decltype(*arg0 == *arg1)* = nullptr) {
        return true;
    }

    template<typename A, typename B>
    static void cmp_help_fun(const A*, const B*, void* = nullptr) { }

    typedef decltype(cmp_help_fun(static_cast<T1*>(nullptr),
                                  static_cast<T2*>(nullptr),
                                  static_cast<bool*>(nullptr)))
            result_type;

 public:

    static constexpr bool value = std::is_same<bool, result_type>::value;

};

} } // namespace cppa::util

#endif // CPPA_IS_COMPARABLE_HPP
