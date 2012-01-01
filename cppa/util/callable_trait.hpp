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


#ifndef CPPA_UTIL_CALLABLE_TRAIT
#define CPPA_UTIL_CALLABLE_TRAIT

#include <type_traits>

#include "cppa/util/type_list.hpp"

namespace cppa { namespace util {

template<typename Signature>
struct callable_trait;

template<class C, typename ResultType, typename... Args>
struct callable_trait<ResultType (C::*)(Args...) const>
{
    typedef ResultType result_type;
    typedef type_list<Args...> arg_types;
};

template<class C, typename ResultType, typename... Args>
struct callable_trait<ResultType (C::*)(Args...)>
{
    typedef ResultType result_type;
    typedef type_list<Args...> arg_types;
};

template<typename ResultType, typename... Args>
struct callable_trait<ResultType (Args...)>
{
    typedef ResultType result_type;
    typedef type_list<Args...> arg_types;
};

template<typename ResultType, typename... Args>
struct callable_trait<ResultType (*)(Args...)>
{
    typedef ResultType result_type;
    typedef type_list<Args...> arg_types;
};

template<bool IsFun, bool IsMemberFun, typename C>
struct get_callable_trait_helper
{
    typedef callable_trait<C> type;
};

template<typename C>
struct get_callable_trait_helper<false, false, C>
{
    typedef callable_trait<decltype(&C::operator())> type;
};

template<typename C>
struct get_callable_trait
{
    typedef typename
            get_callable_trait_helper<
                (   std::is_function<C>::value
                 || std::is_function<typename std::remove_pointer<C>::type>::value),
                std::is_member_function_pointer<C>::value,
                C>::type
            type;
};

} } // namespace cppa::util

#endif // CPPA_UTIL_CALLABLE_TRAIT
