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


#ifndef GUARD_EXPR_HPP
#define GUARD_EXPR_HPP

#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <type_traits>

#include "cppa/util/at.hpp"
#include "cppa/detail/tdata.hpp"

namespace cppa {

enum operator_id
{
    // arithmetic operators
    addition_op, subtraction_op, multiplication_op, division_op, modulo_op,
    // comparison operators
    less_op, less_eq_op, greater_op, greater_eq_op, equal_op, not_equal_op,
    // logical operators
    logical_and_op, logical_or_op,
    // pseudo operators for function invocation
    exec_fun1_op, exec_fun2_op, exec_fun3_op,
    // pseudo operator to store function parameters
    dummy_op
};

// {operator, lhs, rhs} expression
template<operator_id OP, typename First, typename Second>
struct guard_expr
{
    typedef First first_type;
    typedef Second second_type;

    std::pair<First, Second> m_args;

    //guard_expr() = default;

    template<typename T0, typename T1>
    guard_expr(T0&& a0, T1&& a1)
        : m_args(std::forward<T0>(a0), std::forward<T1>(a1))
    {
    }

    // {operator, {operator, a0, a1}, a2}
    template<typename T0, typename T1, typename T2>
    guard_expr(T0&& a0, T1&& a1, T2&& a2)
        : m_args(First{std::forward<T0>(a0), std::forward<T1>(a1)},
                 std::forward<T2>(a2))
    {
    }

    // {operator, {operator, a0, a1}, {operator, a2, a3}}
    template<typename T0, typename T1, typename T2, typename T3>
    guard_expr(T0&& a0, T1&& a1, T2&& a2, T3&& a3)
        : m_args(First{std::forward<T0>(a0), std::forward<T1>(a1)},
                 Second{std::forward<T2>(a2), std::forward<T3>(a3)})
    {
    }

    guard_expr(guard_expr const&) = default;
    guard_expr(guard_expr&& other) : m_args(std::move(other.m_args)) { }
};

#define CPPA_FORALL_OPS(SubMacro)                                              \
    SubMacro (addition_op, +)         SubMacro (subtraction_op, -)             \
    SubMacro (multiplication_op, *)   SubMacro (division_op, /)                \
    SubMacro (modulo_op, %)           SubMacro (less_op, <)                    \
    SubMacro (less_eq_op, <=)         SubMacro (greater_op, >)                 \
    SubMacro (greater_eq_op, >=)      SubMacro (equal_op, ==)                  \
    SubMacro (not_equal_op, !=)


// bind utility for placeholders

template<typename Fun, typename T1>
struct gbind1
{
    typedef guard_expr<exec_fun1_op, Fun, T1> result;
};

template<typename Fun, typename T1, typename T2>
struct gbind2
{
    typedef guard_expr<exec_fun2_op, guard_expr<dummy_op, Fun, T1>, T2> result;
};

template<typename Fun, typename T1, typename T2, typename T3>
struct gbind3
{
    typedef guard_expr<exec_fun3_op, guard_expr<dummy_op, Fun, T1>,
                                     guard_expr<dummy_op, T2, T3> >
            result;
};

/**
 * @brief Call wrapper for guard placeholders and lazy evaluation.
 */
template<typename Fun, typename T1>
typename gbind1<Fun, T1>::result gbind(Fun fun, T1 t1)
{
    return {fun, t1};
}

/**
 * @brief Call wrapper for guard placeholders and lazy evaluation.
 */
template<typename Fun, typename T1, typename T2>
typename gbind2<Fun, T1, T2>::result gbind(Fun fun, T1 t1, T2 t2)
{
    return {fun, t1, t2};
}

/**
 * @brief Call wrapper for guard placeholders and lazy evaluation.
 */
template<typename Fun, typename T1, typename T2, typename T3>
typename gbind3<Fun, T1, T2, T3>::result gbind(Fun fun, T1 t1, T2 t2, T3 t3)
{
    return {fun, t1, t2, t3};
}

struct ge_search_container
{
    bool sc;
    ge_search_container(bool should_contain) : sc(should_contain) { }
    template<class C>
    bool operator()(C const& haystack,
                    typename C::value_type const& needle) const
    {
        typedef typename C::value_type vtype;
        if (sc)
            return std::any_of(haystack.begin(), haystack.end(),
                               [&](vtype const& val) { return needle == val; });
        return std::none_of(haystack.begin(), haystack.end(),
                            [&](vtype const& val) { return needle == val; });
    }

    template<class C>
    bool operator()(std::reference_wrapper<C> const& haystack_ref,
                    typename C::value_type const& needle) const
    {
        return (*this)(haystack_ref.get(), needle);
    }
};

/**
 * @brief A placeholder for guard expression.
 */
template<int X>
struct guard_placeholder
{

    constexpr guard_placeholder() { }

    /**
     * @brief Convenient way to call <tt>gbind(fun, guard_placeholder)</tt>.
     */
    template<typename Fun>
    typename gbind1<Fun, guard_placeholder>::result operator()(Fun fun) const
    {
        return gbind(fun, *this);
    }

    // utility function for starts_with()
    static bool u8_starts_with(std::string const& lhs, std::string const& rhs)
    {
        return std::equal(rhs.begin(), rhs.end(), lhs.begin());
    }

    /**
     * @brief Evaluates to true if unbound argument starts with @p str.
     */
    typename gbind2<decltype(&guard_placeholder::u8_starts_with),
                     guard_placeholder,
                     std::string
             >::result
    starts_with(std::string str) const
    {
        return gbind(&guard_placeholder::u8_starts_with, *this, str);
    }

    /**
     * @brief Evaluates to true if unbound argument
     *        is contained in @p container.
     */
    template<class C>
    typename gbind2<ge_search_container, C, guard_placeholder>::result
    in(C container) const
    {
        return gbind(ge_search_container{true}, container, *this);
    }

    /**
     * @brief Evaluates to true if unbound argument
     *        is contained in @p container.
     */
    template<class C>
    typename gbind2<ge_search_container,
                     std::reference_wrapper<C>,
                     guard_placeholder
             >::result
    in(std::reference_wrapper<C> container) const
    {
        return gbind(ge_search_container{true}, container, *this);
    }

    /**
     * @brief Evaluates to true if unbound argument
     *        is contained in @p list.
     */
    template<typename T>
    typename gbind2<ge_search_container,
                     std::vector<typename detail::strip_and_convert<T>::type>,
                     guard_placeholder
             >::result
    in(std::initializer_list<T> list) const
    {
        std::vector<typename detail::strip_and_convert<T>::type> vec;
        for (auto& i : list) vec.emplace_back(i);
        return in(vec);
    }

    /**
     * @brief Evaluates to true if unbound argument
     *        is not contained in @p container.
     */
    template<class C>
    typename gbind2<ge_search_container, C, guard_placeholder>::result
    not_in(C container) const
    {
        return gbind(ge_search_container{false}, container, *this);
    }

    /**
     * @brief Evaluates to true if unbound argument
     *        is not contained in @p container.
     */
    template<class C>
    typename gbind2<ge_search_container,
                     std::reference_wrapper<C>,
                     guard_placeholder
             >::result
    not_in(std::reference_wrapper<C> container) const
    {
        return gbind(ge_search_container{false}, container, *this);
    }

    /**
     * @brief Evaluates to true if unbound argument
     *        is not contained in @p list.
     */
    template<typename T>
    typename gbind2<ge_search_container,
                     std::vector<typename detail::strip_and_convert<T>::type>,
                     guard_placeholder
             >::result
    not_in(std::initializer_list<T> list) const
    {
        std::vector<typename detail::strip_and_convert<T>::type> vec;
        for (auto& i : list) vec.emplace_back(i);
        return not_in(vec);
    }

};


// result type computation

template<typename T, class Tuple>
struct ge_unbound
{
    typedef T type;
};

// unbound type of placeholder
template<int X, typename... Ts>
struct ge_unbound<guard_placeholder<X>, detail::tdata<Ts...> >
{
    static_assert(X < sizeof...(Ts),
                  "Cannot unbind placeholder (too few arguments)");
    typedef typename std::remove_pointer<typename util::at<X, Ts...>::type>::type type;
};

// operators, operators, operators

#define CPPA_GUARD_PLACEHOLDER_OPERATOR(EnumValue, Operator)                   \
    template<int Pos1, int Pos2>                                               \
    guard_expr< EnumValue , guard_placeholder<Pos1>, guard_placeholder<Pos2>>  \
    operator Operator (guard_placeholder<Pos1> p1, guard_placeholder<Pos2> p2) \
    { return {p1, p2}; }                                                       \
    template<int Pos, typename T>                                              \
    guard_expr< EnumValue , guard_placeholder<Pos>,                            \
                typename detail::strip_and_convert<T>::type >                  \
    operator Operator (guard_placeholder<Pos> gp, T value)                     \
    { return {std::move(gp), std::move(value)}; }                              \
    template<typename T, int Pos>                                              \
    guard_expr< EnumValue ,                                                    \
                typename detail::strip_and_convert<T>::type,                   \
                guard_placeholder<Pos> >                                       \
    operator Operator (T value, guard_placeholder<Pos> gp)                     \
    { return {std::move(value), std::move(gp)}; }

CPPA_FORALL_OPS(CPPA_GUARD_PLACEHOLDER_OPERATOR)

#define CPPA_GUARD_EXPR_OPERATOR(EnumValue, Operator)                          \
    template<operator_id OP, typename F, typename S, typename T>               \
    guard_expr< EnumValue , guard_expr<OP, F, S>,                              \
                            typename detail::strip_and_convert<T>::type>       \
    operator Operator (guard_expr<OP, F, S> lhs, T rhs)                        \
    { return {lhs, rhs}; }                                                     \
    template<typename T, operator_id OP, typename F, typename S>               \
    guard_expr< EnumValue , typename detail::strip_and_convert<T>::type,       \
                            guard_expr<OP, F, S>>                              \
    operator Operator (T lhs, guard_expr<OP, F, S> rhs)                        \
    { return {lhs, rhs}; }                                                     \
    template<operator_id OP, typename F, typename S, int X>                    \
    guard_expr< EnumValue , guard_expr<OP, F, S>, guard_placeholder<X> >       \
    operator Operator (guard_expr<OP, F, S> lhs, guard_placeholder<X> rhs)     \
    { return {lhs, rhs}; }                                                     \
    template<int X, operator_id OP, typename F, typename S>                    \
    guard_expr< EnumValue , guard_placeholder<X>, guard_expr<OP, F, S>>        \
    operator Operator (guard_placeholder<X> lhs, guard_expr<OP, F, S> rhs)     \
    { return {lhs, rhs}; }                                                     \
    template<operator_id OP1, typename F1, typename S1,                        \
             operator_id OP2, typename F2, typename S2>                        \
    guard_expr< EnumValue , guard_expr<OP1, F1, S1>, guard_expr<OP2, F2, S2>>  \
    operator Operator (guard_expr<OP1,F1,S1> lhs, guard_expr<OP2,F2,S2> rhs)   \
    { return {lhs, rhs}; }

CPPA_FORALL_OPS(CPPA_GUARD_EXPR_OPERATOR)

template<operator_id OP>
struct ge_eval_op;

#define CPPA_EVAL_OP_IMPL(EnumValue, Operator)                                 \
    template<> struct ge_eval_op< EnumValue > {                                \
        template<typename T1, typename T2>                                     \
        static inline auto _(T1 const& lhs, T2 const& rhs)                     \
        -> decltype(lhs Operator rhs) { return lhs Operator rhs; }             \
    };

CPPA_FORALL_OPS(CPPA_EVAL_OP_IMPL)
CPPA_EVAL_OP_IMPL(logical_and_op, &&)
CPPA_EVAL_OP_IMPL(logical_or_op, ||)

template<typename T, class Tuple>
struct ge_result_
{
    typedef typename ge_unbound<T, Tuple>::type type;
};

template<operator_id OP, typename First, typename Second, class Tuple>
struct ge_result_<guard_expr<OP, First, Second>, Tuple>
{
    typedef typename ge_result_<First, Tuple>::type lhs_type;
    typedef typename ge_result_<Second, Tuple>::type rhs_type;
    typedef decltype(
            ge_eval_op<OP>::_(*static_cast<lhs_type const*>(nullptr),
                              *static_cast<rhs_type const*>(nullptr))) type;
};

template<typename First, typename Second, class Tuple>
struct ge_result_<guard_expr<exec_fun1_op, First, Second>, Tuple>
{
    typedef First type0;
    typedef typename ge_unbound<Second, Tuple>::type type1;
    typedef decltype(
            (*static_cast<type0 const*>(nullptr))(
                 *static_cast<type1 const*>(nullptr)
             )) type;
};

template<typename First, typename Second, class Tuple>
struct ge_result_<guard_expr<exec_fun2_op, First, Second>, Tuple>
{
    typedef typename First::first_type type0;
    typedef typename ge_unbound<typename First::second_type, Tuple>::type type1;
    typedef typename ge_unbound<Second, Tuple>::type type2;
    typedef decltype(
            (*static_cast<type0 const*>(nullptr))(
                 *static_cast<type1 const*>(nullptr),
                 *static_cast<type2 const*>(nullptr)
             )) type;
};

template<typename First, typename Second, class Tuple>
struct ge_result_<guard_expr<exec_fun3_op, First, Second>, Tuple>
{
    typedef typename First::first_type type0;
    typedef typename ge_unbound<typename First::second_type,Tuple>::type type1;
    typedef typename ge_unbound<typename Second::first_type,Tuple>::type type2;
    typedef typename ge_unbound<typename Second::second_type,Tuple>::type type3;
    typedef decltype(
            (*static_cast<type0 const*>(nullptr))(
                 *static_cast<type1 const*>(nullptr),
                 *static_cast<type2 const*>(nullptr),
                 *static_cast<type3 const*>(nullptr)
             )) type;
};

/*
#define CPPA_G_RESULT_TYPE_SPECIALIZATION(EnumValue, Operator)                 \
    template<typename First, typename Second, class Tuple>                     \
    struct ge_result_< guard_expr< EnumValue , First, Second>, Tuple>          \
    {                                                                          \
        typedef typename ge_result_<First, Tuple>::type lhs_type;              \
        typedef typename ge_result_<Second, Tuple>::type rhs_type;             \
        typedef decltype(*static_cast<lhs_type const*>(nullptr)                \
                         Operator                                              \
                         *static_cast<rhs_type const*>(nullptr)) type;         \
    };

CPPA_G_RESULT_TYPE_SPECIALIZATION(addition_op, +)
CPPA_G_RESULT_TYPE_SPECIALIZATION(subtraction_op, -)
CPPA_G_RESULT_TYPE_SPECIALIZATION(multiplication_op, *)
CPPA_G_RESULT_TYPE_SPECIALIZATION(division_op, /)
CPPA_G_RESULT_TYPE_SPECIALIZATION(modulo_op, %)
*/

template<operator_id OP, typename First, typename Second, class Tuple>
struct ge_result
{
    typedef typename ge_result_<guard_expr<OP, First, Second>, Tuple>::type
            type;
};

template<operator_id OP1, typename F1, typename S1,
         operator_id OP2, typename F2, typename S2>
guard_expr<logical_and_op, guard_expr<OP1, F1, S1>, guard_expr<OP2, F2, S2>>
operator&&(guard_expr<OP1, F1, S1> lhs,
           guard_expr<OP2, F2, S2> rhs)
{
    return {lhs, rhs};
}

template<operator_id OP1, typename F1, typename S1,
         operator_id OP2, typename F2, typename S2>
guard_expr<logical_or_op, guard_expr<OP1, F1, S1>, guard_expr<OP2, F2, S2>>
operator||(guard_expr<OP1, F1, S1> lhs,
           guard_expr<OP2, F2, S2> rhs)
{
    return {lhs, rhs};
}


// evaluation of guard_expr

template<class Tuple, typename T>
inline T const& ge_resolve(Tuple const&, T const& value)
{
    return value;
}

template<class Tuple, int X>
inline auto ge_resolve(Tuple const& tup, guard_placeholder<X>)
            -> decltype(*get<X>(tup))
{
    return *get<X>(tup);
}

template<class Tuple, operator_id OP, typename First, typename Second>
auto ge_resolve(Tuple const& tup, guard_expr<OP, First, Second> const& ge)
     -> typename ge_result<OP, First, Second, Tuple>::type;

template<operator_id OP, class Tuple, typename First, typename Second>
struct ge_eval_
{
    static inline typename ge_result<OP, First, Second, Tuple>::type
    _(Tuple const& tup, First const& lhs, Second const& rhs)
    {
        return ge_eval_op<OP>::_(ge_resolve(tup, lhs), ge_resolve(tup, rhs));
    }
};

template<class Tuple, typename First, typename Second>
struct ge_eval_<logical_and_op, Tuple, First, Second>
{
    static inline bool _(Tuple const& tup, First const& lhs, Second const& rhs)
    {
        // emulate short-circuit evaluation
        if (ge_resolve(tup, lhs)) return ge_resolve(tup, rhs);
        return false;
    }
};

template<class Tuple, typename First, typename Second>
struct ge_eval_<logical_or_op, Tuple, First, Second>
{
    static inline bool _(Tuple const& tup, First const& lhs, Second const& rhs)
    {
        // emulate short-circuit evaluation
        if (ge_resolve(tup, lhs)) return true;
        return ge_resolve(tup, rhs);
    }
};

template<class Tuple, typename First, typename Second>
struct ge_eval_<exec_fun1_op, Tuple, First, Second>
{
    static inline auto _(Tuple const& tup, First const& fun, Second const& arg0)
         -> decltype(fun(ge_resolve(tup, arg0)))
    {
        return fun(ge_resolve(tup, arg0));
    }
};

template<class Tuple, typename First, typename Second>
struct ge_eval_<exec_fun2_op, Tuple, First, Second>
{
    static inline auto _(Tuple const& tup, First const& lhs, Second const& rhs)
         -> decltype(lhs.m_args.first(ge_resolve(tup, lhs.m_args.second),
                                      ge_resolve(tup, rhs)))
    {
        return lhs.m_args.first(ge_resolve(tup, lhs.m_args.second),
                                ge_resolve(tup, rhs));
    }
};

template<class Tuple, typename First, typename Second>
struct ge_eval_<exec_fun3_op, Tuple, First, Second>
{
    static inline auto _(Tuple const& tup, First const& lhs, Second const& rhs)
         -> decltype(lhs.m_args.first(ge_resolve(tup, lhs.m_args.second),
                                      ge_resolve(tup, rhs.m_args.first),
                                      ge_resolve(tup, rhs.m_args.second)))
    {
        return lhs.m_args.first(ge_resolve(tup, lhs.m_args.second),
                                ge_resolve(tup, rhs.m_args.first),
                                ge_resolve(tup, rhs.m_args.second));
    }
};

template<operator_id OP, class Tuple, typename First, typename Second>
inline typename ge_result<OP, First, Second, Tuple>::type
ge_eval(Tuple const& tup, First const& lhs, Second const& rhs)
{
    return ge_eval_<OP, Tuple, First, Second>::_(tup, lhs, rhs);
}

template<class Tuple, operator_id OP, typename First, typename Second>
auto ge_resolve(Tuple const& tup, guard_expr<OP, First, Second> const& ge)
     -> typename ge_result<OP, First, Second, Tuple>::type
{
    return ge_eval<OP>(tup, ge.m_args.first, ge.m_args.second);
}

template<operator_id OP, typename First, typename Second, typename... Args>
auto ge_invoke_step2(guard_expr<OP, First, Second> const& ge,
                     detail::tdata<Args...> const& tup)
     -> typename ge_result<OP, First, Second, detail::tdata<Args...>>::type
{
    return ge_eval<OP>(tup, ge.m_args.first, ge.m_args.second);
}

template<operator_id OP, typename First, typename Second, typename... Args>
auto ge_invoke(guard_expr<OP, First, Second> const& ge,
                     Args const&... args)
     -> typename ge_result<OP, First, Second, detail::tdata<Args*...>>::type
{
    detail::tdata<Args const*...> tup{&args...};
    return ge_invoke_step2(ge, tup);
}

template<class GuardExpr>
struct ge_invoke_helper
{
    GuardExpr const& ge;
    ge_invoke_helper(GuardExpr const& arg) : ge(arg) { }
    template<typename... Args>
    bool operator()(Args&&... args)
    {
        return ge_invoke(ge, std::forward<Args>(args)...);
    }
};

template<typename TupleTypes, operator_id OP, typename First, typename Second>
typename ge_result<
    OP, First, Second,
    typename detail::tdata_from_type_list<
        typename util::tl_filter_not<TupleTypes, is_anything>::type
    >::type
>::type
ge_invoke_any(guard_expr<OP, First, Second> const& ge,
              any_tuple const& tup)
{
    typedef typename ge_result<
                OP, First, Second,
                typename detail::tdata_from_type_list<
                    typename util::tl_filter_not<TupleTypes, is_anything>::type
                >::type
            >::type
            result_type;
    using namespace util;
    typename if_else<
                std::is_same<typename TupleTypes::back, anything>,
                TupleTypes,
                wrapped<typename tl_push_back<TupleTypes, anything>::type>
            >::type
            cast_token;
    auto x = tuple_cast(tup, cast_token);
    CPPA_REQUIRE(static_cast<bool>(x) == true);
    ge_invoke_helper<guard_expr<OP, First, Second> > f{ge};
    return util::unchecked_apply_tuple<result_type>(f, *x);
}

// finally ...

namespace placeholders { namespace {

constexpr guard_placeholder<0> _x1;
constexpr guard_placeholder<1> _x2;
constexpr guard_placeholder<2> _x3;
constexpr guard_placeholder<3> _x4;
constexpr guard_placeholder<4> _x5;
constexpr guard_placeholder<5> _x6;
constexpr guard_placeholder<6> _x7;
constexpr guard_placeholder<7> _x8;
constexpr guard_placeholder<8> _x9;

} } // namespace placeholders::<anonymous>

} // namespace cppa

#endif // GUARD_EXPR_HPP
