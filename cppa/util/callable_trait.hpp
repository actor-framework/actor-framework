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


#ifndef CPPA_UTIL_CALLABLE_TRAIT
#define CPPA_UTIL_CALLABLE_TRAIT

#include <functional>
#include <type_traits>

#include "cppa/util/rm_ref.hpp"
#include "cppa/util/type_list.hpp"

namespace cppa { namespace util {

template<typename Signature>
struct callable_trait;

// member const function pointer
template<class C, typename Result, typename... Ts>
struct callable_trait<Result (C::*)(Ts...) const> {
    typedef Result result_type;
    typedef type_list<Ts...> arg_types;
    typedef std::function<Result (Ts...)> fun_type;
};

// member function pointer
template<class C, typename Result, typename... Ts>
struct callable_trait<Result (C::*)(Ts...)> {
    typedef Result result_type;
    typedef type_list<Ts...> arg_types;
    typedef std::function<Result (Ts...)> fun_type;
};

// good ol' function
template<typename Result, typename... Ts>
struct callable_trait<Result (Ts...)> {
    typedef Result result_type;
    typedef type_list<Ts...> arg_types;
    typedef std::function<Result (Ts...)> fun_type;
};

// good ol' function pointer
template<typename Result, typename... Ts>
struct callable_trait<Result (*)(Ts...)> {
    typedef Result result_type;
    typedef type_list<Ts...> arg_types;
    typedef std::function<Result (Ts...)> fun_type;
};

// matches (IsFun || IsMemberFun)
template<bool IsFun, bool IsMemberFun, typename T>
struct get_callable_trait_helper {
    typedef callable_trait<T> type;
};

// assume functor providing operator()
template<typename C>
struct get_callable_trait_helper<false, false, C> {
    typedef callable_trait<decltype(&C::operator())> type;
};

template<typename T>
struct get_callable_trait {
    // type without cv qualifiers
    typedef typename rm_ref<T>::type bare_type;
    // if type is a function pointer, this typedef identifies the function
    typedef typename std::remove_pointer<bare_type>::type fun_type;
    typedef typename get_callable_trait_helper<
                   std::is_function<bare_type>::value
                || std::is_function<fun_type>::value,
                std::is_member_function_pointer<bare_type>::value,
                bare_type
            >::type
            type;
    typedef typename type::result_type result_type;
    typedef typename type::arg_types arg_types;
};

template<typename C>
struct get_arg_types {
    typedef typename get_callable_trait<C>::type trait_type;
    typedef typename trait_type::arg_types types;
};

} } // namespace cppa::util

#endif // CPPA_UTIL_CALLABLE_TRAIT
