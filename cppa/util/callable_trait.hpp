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

#include "cppa/util/rm_ref.hpp"
#include "cppa/util/type_list.hpp"

namespace cppa { namespace util {

template<typename Signature>
struct callable_trait;

// member function pointer (const)
template<class C, typename ResultType, typename... Args>
struct callable_trait<ResultType (C::*)(Args...) const> {
    typedef ResultType result_type;
    typedef type_list<Args...> arg_types;
};

// member function pointer
template<class C, typename ResultType, typename... Args>
struct callable_trait<ResultType (C::*)(Args...)> {
    typedef ResultType result_type;
    typedef type_list<Args...> arg_types;
};

// good ol' function
template<typename ResultType, typename... Args>
struct callable_trait<ResultType (Args...)> {
    typedef ResultType result_type;
    typedef type_list<Args...> arg_types;
};

// good ol' function pointer
template<typename ResultType, typename... Args>
struct callable_trait<ResultType (*)(Args...)> {
    typedef ResultType result_type;
    typedef type_list<Args...> arg_types;
};

template<bool IsFun, bool IsMemberFun, typename C>
struct get_callable_trait_helper {
    typedef callable_trait<C> type;
};

template<typename C>
struct get_callable_trait_helper<false, false, C> {
    // assuming functor
    typedef callable_trait<decltype(&C::operator())> type;
};

template<typename C>
struct get_callable_trait {
    typedef typename rm_ref<C>::type fun_type;
    typedef typename
            get_callable_trait_helper< (   std::is_function<fun_type>::value
                 || std::is_function<typename std::remove_pointer<fun_type>::type>::value),
                std::is_member_function_pointer<fun_type>::value,
                fun_type>::type
            type;
    typedef typename type::result_type result_type;
    typedef typename type::arg_types arg_types;
};

template<typename C>
struct get_arg_types {
    typedef typename get_callable_trait<C>::type trait_type;
    typedef typename trait_type::arg_types types;
};

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

template<bool IsCallable, typename C>
struct get_result_type_impl {
    typedef typename get_callable_trait<C>::type trait_type;
    typedef typename trait_type::result_type type;
};

template<typename C>
struct get_result_type_impl<false, C> {
    typedef void_type type;
};

template<typename C>
struct get_result_type {
    typedef typename get_result_type_impl<is_callable<C>::value, C>::type type;
};

} } // namespace cppa::util

#endif // CPPA_UTIL_CALLABLE_TRAIT
