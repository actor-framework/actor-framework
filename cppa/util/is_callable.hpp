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


#ifndef CPPA_IS_CALLABLE_HPP
#define CPPA_IS_CALLABLE_HPP

#include "cppa/util/conjunction.hpp"
#include "cppa/util/callable_trait.hpp"

namespace cppa { namespace util {

template<typename T>
struct is_callable {

    template<typename C>
    static bool _fun(C*, typename callable_trait<C>::result_type* = nullptr) {
        return true;
    }

    template<typename C>
    static bool _fun(C*, typename callable_trait<decltype(&C::operator())>::result_type* = nullptr) {
        return true;
    }

    static void _fun(void*) { }

    typedef decltype(_fun(static_cast<typename rm_ref<T>::type*>(nullptr)))
            result_type;

 public:

    static constexpr bool value = std::is_same<bool, result_type>::value;

};

template<typename... Ts>
struct all_callable {
    static constexpr bool value = conjunction<is_callable<Ts>...>::value;
};

} } // namespace cppa::util

#endif // CPPA_IS_CALLABLE_HPP
