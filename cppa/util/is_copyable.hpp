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


#ifndef IS_COPYABLE_HPP
#define IS_COPYABLE_HPP

#include <type_traits>

namespace cppa { namespace detail {

template<bool IsAbstract, typename T>
class is_copyable_
{

    template<typename A>
    static bool cpy_help_fun(A const* arg0, decltype(new A(*arg0)) = nullptr)
    {
        return true;
    }

    template<typename A>
    static void cpy_help_fun(A const*, void* = nullptr) { }

    typedef decltype(cpy_help_fun(static_cast<T*>(nullptr),
                                  static_cast<T*>(nullptr)))
            result_type;

 public:

    static const bool value = std::is_same<bool, result_type>::value;

};

template<typename T>
class is_copyable_<true, T>
{

 public:

    static const bool value = false;

};

} } // namespace cppa::detail

namespace cppa { namespace util {

template<typename T>
struct is_copyable : std::integral_constant<bool, detail::is_copyable_<std::is_abstract<T>::value, T>::value>
{
};

} } // namespace cppa::util

#endif // IS_COPYABLE_HPP
