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


#ifndef CPPA_APPLY_ARGS_HPP
#define CPPA_APPLY_ARGS_HPP

#include <cstddef>

namespace cppa { namespace util {

template<typename Result, size_t NumFunctorArgs, size_t NumArgs>
struct apply_args {
    template<class Fun, typename Arg0, typename... Args>
    static Result _(const Fun& fun, Arg0&&, Args&&... args) {
        return apply_args<Result, NumFunctorArgs, sizeof...(Args)>
               ::_(fun, std::forward<Args>(args)...);
    }
};

template<typename Result, size_t X>
struct apply_args<Result, X, X> {
    template<class Fun, typename... Args>
    static Result _(const Fun& fun, Args&&... args) {
        return fun(std::forward<Args>(args)...);
    }
};

} } // namespace cppa::util

#endif // CPPA_APPLY_ARGS_HPP
