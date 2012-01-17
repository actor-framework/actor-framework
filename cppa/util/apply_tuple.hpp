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


#ifndef APPLY_TUPLE_HPP
#define APPLY_TUPLE_HPP

#include "cppa/util/enable_if.hpp"
#include "cppa/util/disable_if.hpp"
#include "cppa/util/is_manipulator.hpp"
#include "cppa/util/callable_trait.hpp"

namespace cppa { namespace util {

template<size_t... Range>
struct apply_tuple_impl
{
    template<typename F, template<typename...> class Tuple, typename... T>
    static auto apply(F&& f, Tuple<T...> const& args,
                      typename disable_if<is_manipulator<F>, int>::type = 0)
        -> typename get_result_type<F>::type
    {
        return f(get<Range>(args)...);
    }
    template<typename F, template<typename...> class Tuple, typename... T>
    static auto apply(F&& f, Tuple<T...>& args,
                      typename enable_if<is_manipulator<F>, int>::type = 0)
        -> typename get_result_type<F>::type
    {
        return f(get_ref<Range>(args)...);
    }
};

template<int From, int To, int... Args>
struct apply_tuple_util : apply_tuple_util<From, To-1, To, Args...>
{
};

template<int X, int... Args>
struct apply_tuple_util<X, X, Args...> : apply_tuple_impl<X, Args...>
{
};

template<typename F, template<typename...> class Tuple, typename... T>
auto apply_tuple(F&& fun, Tuple<T...>& tup)
    -> typename get_result_type<F>::type
{
    typedef typename get_arg_types<F>::types fun_args;
    static constexpr size_t tup_size = sizeof...(T);
    static_assert(tup_size >= fun_args::size,
                  "cannot conjure up additional arguments");
    static constexpr size_t from = tup_size - fun_args::size;
    static constexpr size_t to = tup_size - 1;
    return apply_tuple_util<from, to>::apply(std::forward<F>(fun), tup);
}

template<typename F, template<typename...> class Tuple, typename... T>
auto apply_tuple(F&& fun, Tuple<T...> const& tup)
    -> typename get_result_type<F>::type
{
    typedef typename get_arg_types<F>::types fun_args;
    static constexpr size_t tup_size = sizeof...(T);
    static_assert(tup_size >= fun_args::size,
                  "cannot conjure up additional arguments");
    static constexpr size_t from = tup_size - fun_args::size;
    static constexpr size_t to = tup_size - 1;
    return apply_tuple_util<from, to>::apply(std::forward<F>(fun), tup);
}

} } // namespace cppa::util

#endif // APPLY_TUPLE_HPP
