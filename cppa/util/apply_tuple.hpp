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


#ifndef CPPA_APPLY_TUPLE_HPP
#define CPPA_APPLY_TUPLE_HPP

#include <type_traits>

#include "cppa/get.hpp"
#include "cppa/util/is_manipulator.hpp"
#include "cppa/util/callable_trait.hpp"

namespace cppa { namespace util {

template<typename Result, bool IsManipulator, size_t... Range>
struct apply_tuple_impl {
    template<typename F, class Tuple>
    static inline Result apply(F& f, const Tuple& args) {
        return f(get<Range>(args)...);
    }
};

template<typename Result, size_t... Range>
struct apply_tuple_impl<Result, true, Range...> {
    template<typename F, class Tuple>
    static inline Result apply(F& f, Tuple& args) {
        return f(get_ref<Range>(args)...);
    }
};

template<typename Result, bool IsManipulator, int From, int To, int... Args>
struct apply_tuple_util
        : apply_tuple_util<Result, IsManipulator, From, To-1, To, Args...> {
};

template<typename Result, bool IsManipulator, int X, int... Args>
struct apply_tuple_util<Result, IsManipulator, X, X, Args...>
        : apply_tuple_impl<Result, IsManipulator, X, Args...> {
};

template<typename Result, bool IsManipulator>
struct apply_tuple_util<Result, IsManipulator, 1, 0> {
    template<typename F, class Unused>
    static Result apply(F& f, const Unused&) {
        return f();
    }
};

template<typename F, template<typename...> class Tuple, typename... T>
typename get_result_type<F>::type apply_tuple(F&& fun, Tuple<T...>& tup) {
    typedef typename get_result_type<F>::type result_type;
    typedef typename get_arg_types<F>::types fun_args;
    static constexpr size_t tup_size = sizeof...(T);
    static_assert(tup_size >= fun_args::size,
                  "cannot conjure up additional arguments");
    static constexpr size_t args_size = fun_args::size;
    static constexpr size_t from = (args_size > 0) ? tup_size - args_size : 1;
    static constexpr size_t to = (args_size > 0) ? tup_size - 1 : 0;
    return apply_tuple_util<result_type, is_manipulator<F>::value, from, to>
           ::apply(std::forward<F>(fun), tup);
}

template<typename F, template<typename...> class Tuple, typename... T>
typename get_result_type<F>::type apply_tuple(F&& fun, const Tuple<T...>& tup) {
    typedef typename get_result_type<F>::type result_type;
    typedef typename get_arg_types<F>::types fun_args;
    static constexpr size_t tup_size = sizeof...(T);
    static_assert(tup_size >= fun_args::size,
                  "cannot conjure up additional arguments");
    static constexpr size_t args_size = fun_args::size;
    static constexpr size_t from = (args_size > 0) ? tup_size - args_size : 1;
    static constexpr size_t to = (args_size > 0) ? tup_size - 1 : 0;
    return apply_tuple_util<result_type, is_manipulator<F>::value, from, to>
           ::apply(std::forward<F>(fun), tup);
}

template<typename Result, size_t From, size_t To,
         typename F, template<typename...> class Tuple, typename... T>
Result unchecked_apply_tuple_in_range(F&& fun, const Tuple<T...>& tup) {
    return apply_tuple_util<Result, false, From, To>
           ::apply(std::forward<F>(fun), tup);
}

template<typename Result, size_t From, size_t To,
         typename F, template<typename...> class Tuple, typename... T>
Result unchecked_apply_tuple_in_range(F&& fun, Tuple<T...>& tup) {
    return apply_tuple_util<Result, true, From, To>
           ::apply(std::forward<F>(fun), tup);
}

// applies all values of @p tup to @p fun
// does not evaluate result type of functor
template<typename Result, typename F,
         template<typename...> class Tuple, typename... T>
Result unchecked_apply_tuple(F&& fun, const Tuple<T...>& tup) {
    return apply_tuple_util<Result, false, 0, sizeof...(T) - 1>
           ::apply(std::forward<F>(fun), tup);
}

// applies all values of @p tup to @p fun
// does not evaluate result type of functor
template<typename Result, typename F,
         template<typename...> class Tuple, typename... T>
Result unchecked_apply_tuple(F&& fun, Tuple<T...>& tup) {
    return apply_tuple_util<Result, true, 0, sizeof...(T) - 1>
           ::apply(std::forward<F>(fun), tup);
}

} } // namespace cppa::util

#endif // CPPA_APPLY_TUPLE_HPP
