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


#ifndef CPPA_TPARTIAL_FUNCTION_HPP
#define CPPA_TPARTIAL_FUNCTION_HPP

#include <cstddef>
#include <type_traits>

#include "cppa/util/rm_ref.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/apply_args.hpp"
#include "cppa/util/left_or_right.hpp"
#include "cppa/util/callable_trait.hpp"
#include "cppa/util/is_mutable_ref.hpp"

namespace cppa {

template<class Expr, class Guard, typename Result, typename... Args>
class tpartial_function {

    typedef typename util::get_callable_trait<Expr>::type ctrait;
    typedef typename ctrait::arg_types ctrait_args;

    static_assert(util::tl_exists<util::type_list<Args...>,
                                  std::is_rvalue_reference >::value == false,
                  "partial functions using rvalue arguments are not supported");

 public:

    typedef util::type_list<Args...> arg_types;

    static constexpr size_t num_arguments = sizeof...(Args);

    static constexpr bool manipulates_args =
            util::tl_exists<arg_types, util::is_mutable_ref>::value;

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

    //bool defined_at(const typename util::rm_ref<Args>::type&... args) const
    bool defined_at(Args... args) const {
        return m_guard(args...);
    }

    Result operator()(Args... args) const {
        return util::apply_args<Result, ctrait_args::size, sizeof...(Args)>
               ::_(m_expr, args...);
    }

 private:

    Guard m_guard;
    Expr m_expr;

};

template<class Expr, class Guard, typename Args,
         typename Result = util::void_type, size_t Step = 0>
struct get_tpartial_function;

template<class Expr, class Guard, typename... Args, typename Result>
struct get_tpartial_function<Expr, Guard, util::type_list<Args...>, Result, 1> {
    typedef tpartial_function<Expr, Guard, Result, Args...> type;
};

template<class Expr, class Guard, typename... Args>
struct get_tpartial_function<Expr, Guard,
                             util::type_list<Args...>, util::void_type, 0> {
    typedef typename util::get_callable_trait<Expr>::type ctrait;
    typedef typename ctrait::arg_types arg_types;

    static_assert(arg_types::size <= sizeof...(Args),
                  "Functor takes too much arguments");

    typedef typename get_tpartial_function<
                Expr,
                Guard,
                // fill arg_types of Expr from left with const Args&
                typename util::tl_zip<
                    typename util::tl_pad_left<
                        typename ctrait::arg_types,
                        sizeof...(Args)
                    >::type,
                    util::type_list<const Args&...>,
                    util::left_or_right
                >::type,
                typename ctrait::result_type,
                1
            >::type
            type;
};

} // namespace cppa

#endif // CPPA_TPARTIAL_FUNCTION_HPP
