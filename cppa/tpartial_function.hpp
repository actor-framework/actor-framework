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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#ifndef CPPA_TPARTIAL_FUNCTION_HPP
#define CPPA_TPARTIAL_FUNCTION_HPP

#include <cstddef>
#include <type_traits>

#include "cppa/none.hpp"
#include "cppa/unit.hpp"
#include "cppa/optional.hpp"

#include "cppa/util/call.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/type_traits.hpp"
#include "cppa/util/left_or_right.hpp"

namespace cppa {

template<typename Result, typename... Ts>
struct tpartial_function_helper {
    template<typename Fun>
    inline optional<Result> operator()(const Fun& fun, Ts... args) {
        if (fun.defined_at(args...)) return fun.invoke(args...);
        return none;
    }
};

template<typename... Ts>
struct tpartial_function_helper<void, Ts...> {
    template<typename Fun>
    inline optional<void> operator()(const Fun& fun, Ts... args) {
        if (fun.defined_at(args...)) {
            fun.invoke(args...);
            return unit;
        }
        return none;
    }
};


template<class Expr, class Guard, typename Result, typename... Ts>
class tpartial_function {

    typedef typename util::get_callable_trait<Expr>::type ctrait;

    typedef typename ctrait::arg_types ctrait_args;

    static constexpr size_t num_expr_args = util::tl_size<ctrait_args>::value;

    static_assert(util::tl_exists<util::type_list<Ts...>,
                                  std::is_rvalue_reference >::value == false,
                  "partial functions using rvalue arguments are not supported");

 public:

    typedef util::type_list<Ts...> arg_types;

    typedef Guard guard_type;

    static constexpr size_t num_arguments = sizeof...(Ts);

    static constexpr bool manipulates_args =
            util::tl_exists<arg_types, util::is_mutable_ref>::value;

    typedef Result result_type;

    template<typename Fun, typename... G>
    tpartial_function(Fun&& fun, G&&... guard_args)
        : m_guard(std::forward<G>(guard_args)...)
        , m_expr(std::forward<Fun>(fun)) {
    }

    tpartial_function(tpartial_function&& other)
        : m_guard(std::move(other.m_guard))
        , m_expr(std::move(other.m_expr)) {
    }

    tpartial_function(const tpartial_function&) = default;

    inline bool defined_at(Ts... args) const {
        return m_guard(args...);
    }

    /**
     * @brief Invokes the function without evaluating the guard.
     */
    result_type invoke(Ts... args) const {
        auto targs = std::forward_as_tuple(args...);
        auto indices = util::get_right_indices<num_expr_args>(targs);
        return util::apply_args(m_expr, targs, indices);
    }

    inline optional<result_type> operator()(Ts... args) const {
        tpartial_function_helper<result_type, Ts...> helper;
        return helper(*this, args...);
    }

 private:

    Guard m_guard;
    Expr m_expr;

};

template<class Expr, class Guard, typename Ts,
         typename Result = unit_t, size_t Step = 0>
struct get_tpartial_function;

template<class Expr, class Guard, typename... Ts, typename Result>
struct get_tpartial_function<Expr, Guard, util::type_list<Ts...>, Result, 1> {
    typedef tpartial_function<Expr, Guard, Result, Ts...> type;
};

template<class Expr, class Guard, typename... Ts>
struct get_tpartial_function<Expr, Guard,
                             util::type_list<Ts...>, unit_t, 0> {
    typedef typename util::get_callable_trait<Expr>::type ctrait;
    typedef typename ctrait::arg_types arg_types;

    static_assert(util::tl_size<arg_types>::value <= sizeof...(Ts),
                  "Functor takes too much arguments");

    typedef typename get_tpartial_function<
                Expr,
                Guard,
                // fill arg_types of Expr from left with const Ts&
                typename util::tl_zip<
                    typename util::tl_pad_left<
                        typename ctrait::arg_types,
                        sizeof...(Ts)
                    >::type,
                    util::type_list<const Ts&...>,
                    util::left_or_right
                >::type,
                typename ctrait::result_type,
                1
            >::type
            type;
};

} // namespace cppa

#endif // CPPA_TPARTIAL_FUNCTION_HPP
