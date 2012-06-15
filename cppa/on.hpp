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


#ifndef CPPA_ON_HPP
#define CPPA_ON_HPP

#include <chrono>
#include <memory>
#include <type_traits>

#include "cppa/atom.hpp"
#include "cppa/pattern.hpp"
#include "cppa/anything.hpp"
#include "cppa/behavior.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/guard_expr.hpp"
#include "cppa/match_expr.hpp"
#include "cppa/partial_function.hpp"

#include "cppa/util/duration.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/arg_match_t.hpp"
#include "cppa/util/callable_trait.hpp"

#include "cppa/detail/boxed.hpp"
#include "cppa/detail/unboxed.hpp"
#include "cppa/detail/value_guard.hpp"
#include "cppa/detail/ref_counted_impl.hpp"
#include "cppa/detail/implicit_conversions.hpp"

namespace cppa { namespace detail {

template<bool IsFun, typename T>
struct add_ptr_to_fun_ {
    typedef T* type;
};

template<typename T>
struct add_ptr_to_fun_<false, T> {
    typedef T type;
};

template<typename T>
struct add_ptr_to_fun : add_ptr_to_fun_<std::is_function<T>::value, T> {
};

template<bool ToVoid, typename T>
struct to_void_impl {
    typedef util::void_type type;
};

template<typename T>
struct to_void_impl<false, T> {
    typedef typename add_ptr_to_fun<T>::type type;
};

template<typename T>
struct not_callable_to_void : to_void_impl<detail::is_boxed<T>::value || !util::is_callable<T>::value, T> {
};

template<typename T>
struct boxed_and_callable_to_void : to_void_impl<detail::is_boxed<T>::value || util::is_callable<T>::value, T> {
};

class behavior_rvalue_builder {

    util::duration m_timeout;

 public:

    constexpr behavior_rvalue_builder(const util::duration& d) : m_timeout(d) {
    }

    template<typename F>
    behavior operator>>(F&& f) {
        return {m_timeout, std::function<void()>{std::forward<F>(f)}};
    }

};

struct rvalue_builder_args_ctor { };

template<class Left, class Right>
struct disjunct_rvalue_builders {
    Left m_left;
    Right m_right;

 public:

    disjunct_rvalue_builders(Left l, Right r) : m_left(std::move(l))
                                              , m_right(std::move(r)) {
    }

    template<typename Expr>
    auto operator>>(Expr expr)
         -> decltype((*(static_cast<Left*>(nullptr)) >> expr).or_else(
                      *(static_cast<Right*>(nullptr)) >> expr)) const {
        return (m_left >> expr).or_else(m_right >> expr);
    }

};

template<class Guard, class Transformers, class Pattern>
struct rvalue_builder {

    static constexpr bool is_complete =
            !std::is_same<util::arg_match_t, typename Pattern::back>::value;

    typedef typename util::tl_apply<Transformers, tdata>::type fun_container;

    Guard m_guard;
    fun_container m_funs;

 public:

    rvalue_builder() = default;

    template<typename... Args>
    rvalue_builder(rvalue_builder_args_ctor, const Args&... args)
        : m_guard(args...)
        , m_funs(args...) {
    }

    rvalue_builder(Guard arg0, fun_container arg1)
        : m_guard(std::move(arg0)), m_funs(std::move(arg1)) {
    }

    template<typename NewGuard>
    rvalue_builder<
        guard_expr<
            logical_and_op,
            guard_expr<exec_xfun_op, Guard, util::void_type>,
            NewGuard>,
        Transformers,
        Pattern>
    when(NewGuard ng,
         typename std::enable_if<
               std::is_same<NewGuard, NewGuard>::value
            && !std::is_same<Guard, value_guard< util::type_list<> >>::value
         >::type* = 0                                 ) const {
        return {(ge_sub_function(m_guard) && ng), std::move(m_funs)};
    }

    template<typename NewGuard>
    rvalue_builder<NewGuard, Transformers, Pattern>
    when(NewGuard ng,
         typename std::enable_if<
               std::is_same<NewGuard, NewGuard>::value
            && std::is_same<Guard, value_guard< util::type_list<> >>::value
         >::type* = 0                                 ) const {
        return {std::move(ng), std::move(m_funs)};
    }

    template<typename Expr>
    match_expr<typename get_case<is_complete, Expr, Guard, Transformers, Pattern>::type>
    operator>>(Expr expr) const {
        typedef typename get_case<
                    is_complete,
                    Expr,
                    Guard,
                    Transformers,
                    Pattern
                >::type
                tpair;
        return tpair{typename tpair::first_type{m_funs},
                     typename tpair::second_type{std::move(expr),
                                                 std::move(m_guard)}};
    }

    template<class Other>
    disjunct_rvalue_builders<rvalue_builder, Other> operator||(Other other) const {
        return {*this, std::move(other)};
    }

};

template<bool IsCallable, typename T>
struct pattern_type_ {
    typedef util::get_callable_trait<T> ctrait;
    typedef typename ctrait::arg_types args;
    static_assert(args::size == 1, "only unary functions allowed");
    typedef typename util::rm_ref<typename args::head>::type type;
};

template<typename T>
struct pattern_type_<false, T> {
    typedef typename implicit_conversions<
                typename util::rm_ref<
                    typename detail::unboxed<T>::type
                >::type
            >::type
            type;
};

template<typename T>
struct pattern_type : pattern_type_<util::is_callable<T>::value && !detail::is_boxed<T>::value, T> {
};


} } // cppa::detail

namespace cppa {

#ifdef CPPA_DOCUMENTATION

/**
 * @brief A wildcard that matches the argument types
*         of a given callback. Must be the last argument to {@link on()}.
 * @see {@link math_actor_example.cpp Math Actor Example}
 */
constexpr ___ arg_match;

/**
 * @brief Left-hand side of a partial function expression.
 *
 * Equal to <tt>on(arg_match)</tt>.
 */
constexpr ___ on_arg_match;

/**
 * @brief A wildcard that matches any value of type @p T.
 * @see {@link math_actor_example.cpp Math Actor Example}
 */
template<typename T>
___ val();

/**
 * @brief A wildcard that matches any number of any values.
 */
constexpr anything any_vals;


/**
 * @brief Left-hand side of a partial function expression that matches values.
 *
 * This overload can be used with the wildcards {@link cppa::val val},
 * {@link cppa::any_vals any_vals} and {@link cppa::arg_match arg_match}.
 */
template<typename Arg0, typename... Args>
___ on(const Arg0& arg0, const Args&... args);

/**
 * @brief Left-hand side of a partial function expression that matches types.
 *
 * This overload matches types only. The type {@link cppa::anything anything}
 * can be used as wildcard to match any number of elements of any types.
 */
template<typename... Ts>
___ on();

/**
 * @brief Left-hand side of a partial function expression that matches types.
 *
 * This overload matches up to four leading atoms.
 * The type {@link cppa::anything anything}
 * can be used as wildcard to match any number of elements of any types.
 */
template<atom_value... Atoms, typename... Ts>
___ on();

#else

template<typename T>
constexpr typename detail::boxed<T>::type val() {
    return typename detail::boxed<T>::type();
}

constexpr anything any_vals = anything();

typedef typename detail::boxed<util::arg_match_t>::type boxed_arg_match_t;

constexpr boxed_arg_match_t arg_match = boxed_arg_match_t();

template<typename Arg0, typename... Args>
detail::rvalue_builder<
    detail::value_guard<
        typename util::tl_trim<
            typename util::tl_map<
                util::type_list<Arg0, Args...>,
                detail::boxed_and_callable_to_void,
                detail::implicit_conversions
            >::type
        >::type
    >,
    typename util::tl_map<
        util::type_list<Arg0, Args...>,
        detail::not_callable_to_void
    >::type,
    util::type_list<typename detail::pattern_type<Arg0>::type,
                    typename detail::pattern_type<Args>::type...> >
on(const Arg0& arg0, const Args&... args) {
    return {detail::rvalue_builder_args_ctor{}, arg0, args...};
}

template<typename... T>
detail::rvalue_builder<detail::value_guard<util::type_list<> >,
                      util::type_list<>,
                      util::type_list<T...> >
on() {
    return {};
}

template<atom_value A0, typename... Ts>
decltype(on(A0, val<Ts>()...)) on() {
    return on(A0, val<Ts>()...);
}

template<atom_value A0, atom_value A1, typename... Ts>
decltype(on(A0, A1, val<Ts>()...)) on() {
    return on(A0, A1, val<Ts>()...);
}

template<atom_value A0, atom_value A1, atom_value A2, typename... Ts>
decltype(on(A0, A1, A2, val<Ts>()...)) on() {
    return on(A0, A1, A2, val<Ts>()...);
}

template<atom_value A0, atom_value A1, atom_value A2, atom_value A3,
         typename... Ts>
decltype(on(A0, A1, A2, A3, val<Ts>()...)) on() {
    return on(A0, A1, A2, A3, val<Ts>()...);
}

template<class Rep, class Period>
constexpr detail::behavior_rvalue_builder
after(const std::chrono::duration<Rep, Period>& d) {
    return { util::duration(d) };
}

inline decltype(on<anything>()) others() {
    return on<anything>();
}

// some more convenience

namespace detail {

class on_the_fly_rvalue_builder {

 public:

    constexpr on_the_fly_rvalue_builder() {
    }

    template<typename Guard>
    auto when(Guard g) const -> decltype(on(arg_match).when(g)) {
        return on(arg_match).when(g);
    }

    template<typename Expr>
    match_expr<
        typename get_case<
            false,
            Expr,
            value_guard< util::type_list<> >,
            util::type_list<>,
            util::type_list<>
        >::type>
    operator>>(Expr expr) const {
        typedef typename get_case<
                    false,
                    Expr,
                    value_guard< util::type_list<> >,
                    util::type_list<>,
                    util::type_list<>
                >::type
                result;
        return result{typename result::first_type{},
                      typename result::second_type{
                            std::move(expr),
                            value_guard< util::type_list<> >{}}};
    }

};

} // namespace detail

constexpr detail::on_the_fly_rvalue_builder on_arg_match;

#endif

} // namespace cppa

#endif // CPPA_ON_HPP
