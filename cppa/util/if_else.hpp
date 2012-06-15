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


#ifndef CPPA_IF_ELSE_HPP
#define CPPA_IF_ELSE_HPP

#include "cppa/util/wrapped.hpp"

namespace cppa { namespace util {

// if (IfStmt == true) type = T; else type = Else::type;
template<bool IfStmt, typename T, class Else>
struct if_else_c {
    typedef T type;
};

template<typename T, class Else>
struct if_else_c<false, T, Else> {
    typedef typename Else::type type;
};

/**
 * @brief A conditinal expression for types that allows
 *        nested statements (unlike std::conditional).
 *
 * @c type is defined as @p T if <tt>IfStmt == true</tt>;
 * otherwise @c type is defined as @p Else::type.
 */
template<class Stmt, typename T, class Else>
struct if_else : if_else_c<Stmt::value, T, Else> { };

} } // namespace cppa::util

#endif // CPPA_IF_ELSE_HPP
