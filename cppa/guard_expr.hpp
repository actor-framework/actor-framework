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


#ifndef CPPA_GUARD_EXPR_HPP
#define CPPA_GUARD_EXPR_HPP

#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <type_traits>

#include "cppa/config.hpp"
#include "cppa/option.hpp"

#include "cppa/util/at.hpp"
#include "cppa/util/rm_ref.hpp"
#include "cppa/util/void_type.hpp"
#include "cppa/util/apply_tuple.hpp"

#include "cppa/detail/tdata.hpp"

namespace cppa {

enum operator_id {
    // arithmetic operators
    addition_op, subtraction_op, multiplication_op, division_op, modulo_op,
    // comparison operators
    less_op, less_eq_op, greater_op, greater_eq_op, equal_op, not_equal_op,
    // logical operators
    logical_and_op, logical_or_op,
    // pseudo operators for function invocation
    exec_fun1_op, exec_fun2_op, exec_fun3_op,
    // operator to invoke a given functor with all arguments forwarded
    exec_xfun_op,
    // pseudo operator to store function parameters
    dummy_op
};

// {operator, lhs, rhs} expression
template<operator_id OP, typename First, typename Second>
struct guard_expr {
    typedef First first_type;
    typedef Second second_type;

    std::pair<First, Second> m_args;

    //guard_expr() = default;

    template<typename T0, typename T1>
    guard_expr(T0&& a0, T1&& a1)
        : m_args(std::forward<T0>(a0), std::forward<T1>(a1)) {
    }

    // {operator, {operator, a0, a1}, a2}
    template<typename T0, typename T1, typename T2>
    guard_expr(T0&& a0, T1&& a1, T2&& a2)
        : m_args(First{std::forward<T0>(a0), std::forward<T1>(a1)},
                 std::forward<T2>(a2)) {
    }

    // {operator, {operator, a0, a1}, {operator, a2, a3}}
    template<typename T0, typename T1, typename T2, typename T3>
    guard_expr(T0&& a0, T1&& a1, T2&& a2, T3&& a3)
        : m_args(First{std::forward<T0>(a0), std::forward<T1>(a1)},
                 Second{std::forward<T2>(a2), std::forward<T3>(a3)}) {
    }

    guard_expr(const guard_expr&) = default;
    guard_expr(guard_expr&& other) : m_args(std::move(other.m_args)) { }

    template<typename... Args>
    bool operator()(const Args&... args) const;

};

#define CPPA_FORALL_OPS(SubMacro)                                              \
    SubMacro (addition_op, +)         SubMacro (subtraction_op, -)             \
    SubMacro (multiplication_op, *)   SubMacro (division_op, /)                \
    SubMacro (modulo_op, %)           SubMacro (less_op, <)                    \
    SubMacro (less_eq_op, <=)         SubMacro (greater_op, >)                 \
    SubMacro (greater_eq_op, >=)      SubMacro (equal_op, ==)                  \
    SubMacro (not_equal_op, !=)


template<typename T>
struct ge_mutable_reference_wrapper {
    T* value;
    ge_mutable_reference_wrapper() : value(nullptr) { }
    ge_mutable_reference_wrapper(T&&) = delete;
    ge_mutable_reference_wrapper(T& vref) : value(&vref) { }
    ge_mutable_reference_wrapper(const ge_mutable_reference_wrapper&) = default;
    ge_mutable_reference_wrapper& operator=(T& vref) {
        value = &vref;
        return *this;
    }
    ge_mutable_reference_wrapper& operator=(const ge_mutable_reference_wrapper&) = default;
    T& get() const { CPPA_REQUIRE(value != 0); return *value; }
    operator T& () const { CPPA_REQUIRE(value != 0); return *value; }
};

template<typename T>
struct ge_reference_wrapper {
    const T* value;
    ge_reference_wrapper(T&&) = delete;
    ge_reference_wrapper() : value(nullptr) { }
    ge_reference_wrapper(const T& val_ref) : value(&val_ref) { }
    ge_reference_wrapper(const ge_reference_wrapper&) = default;
    ge_reference_wrapper& operator=(const T& vref) {
        value = &vref;
        return *this;
    }
    ge_reference_wrapper& operator=(const ge_reference_wrapper&) = default;
    const T& get() const { CPPA_REQUIRE(value != 0); return *value; }
    operator const T& () const { CPPA_REQUIRE(value != 0); return *value; }
};

// support use of gref(BooleanVariable) as receive loop 'guard'
template<>
struct ge_reference_wrapper<bool> {
    const bool* value;
    ge_reference_wrapper(bool&&) = delete;
    ge_reference_wrapper(const bool& val_ref) : value(&val_ref) { }
    ge_reference_wrapper(const ge_reference_wrapper&) = default;
    ge_reference_wrapper& operator=(const ge_reference_wrapper&) = default;
    const bool& get() const { CPPA_REQUIRE(value != 0); return *value; }
    operator const bool& () const { CPPA_REQUIRE(value != 0); return *value; }
    bool operator()() const { CPPA_REQUIRE(value != 0); return *value; }
};

/**
 * @brief Creates a reference wrapper similar to std::reference_wrapper<const T>
 *        that could be used in guard expressions or to enforce lazy evaluation.
 */
template<typename T>
ge_reference_wrapper<T> gref(const T& value) { return {value}; }

// bind utility for placeholders

template<typename Fun, typename T1>
struct gcall1 {
    typedef guard_expr<exec_fun1_op, Fun, T1> result;
};

template<typename Fun, typename T1, typename T2>
struct gcall2 {
    typedef guard_expr<exec_fun2_op, guard_expr<dummy_op, Fun, T1>, T2> result;
};

template<typename Fun, typename T1, typename T2, typename T3>
struct gcall3 {
    typedef guard_expr<exec_fun3_op, guard_expr<dummy_op, Fun, T1>,
                                     guard_expr<dummy_op, T2, T3> >
            result;
};

/**
 * @brief Call wrapper for guard placeholders and lazy evaluation.
 */
template<typename Fun, typename T1>
typename gcall1<Fun, T1>::result gcall(Fun fun, T1 t1) {
    return {fun, t1};
}

/**
 * @brief Call wrapper for guard placeholders and lazy evaluation.
 */
template<typename Fun, typename T1, typename T2>
typename gcall2<Fun, T1, T2>::result gcall(Fun fun, T1 t1, T2 t2) {
    return {fun, t1, t2};
}

/**
 * @brief Call wrapper for guard placeholders and lazy evaluation.
 */
template<typename Fun, typename T1, typename T2, typename T3>
typename gcall3<Fun, T1, T2, T3>::result gcall(Fun fun, T1 t1, T2 t2, T3 t3) {
    return {fun, t1, t2, t3};
}

/**
 * @brief Calls @p fun with all arguments given to the guard expression.
 *        The functor @p fun must return a boolean.
 */
template<typename Fun>
guard_expr<exec_xfun_op, Fun, util::void_type> ge_sub_function(Fun fun) {
    return {fun, util::void_type{}};
}

struct ge_search_container {
    bool sc;
    ge_search_container(bool should_contain) : sc(should_contain) { }

    template<class C>
    bool operator()(const C& haystack,
                    const typename C::value_type& needle) const {
        typedef typename C::value_type vtype;
        if (sc)
            return std::any_of(haystack.begin(), haystack.end(),
                               [&](const vtype& val) { return needle == val; });
        return std::none_of(haystack.begin(), haystack.end(),
                            [&](const vtype& val) { return needle == val; });
    }
};

struct ge_get_size {
    template<class C>
    inline auto operator()(const C& what) const -> decltype(what.size()) {
        return what.size();
    }
};

struct ge_is_empty {
    bool expected;
    ge_is_empty(bool expected_value) : expected(expected_value) { }
    template<class C>
    inline bool operator()(const C& what) const {
        return what.empty() == expected;
    }
};

struct ge_get_front {
    template<class C>
    inline auto operator()(const C& what,
                           typename std::enable_if<
                               std::is_reference<
                                   decltype(what.front())
                               >::value
                           >::type* = 0) const
    -> option<
        std::reference_wrapper<
            const typename util::rm_ref<decltype(what.front())>::type> > {
        if (what.empty() == false) return {what.front()};
        return {};
    }
    template<class C>
    inline auto operator()(const C& what,
                           typename std::enable_if<
                               std::is_reference<
                                   decltype(what.front())
                               >::value == false
                           >::type* = 0) const
    -> option<decltype(what.front())> {
        if (what.empty() == false) return {what.front()};
        return {};
    }
};

/**
 * @brief A placeholder for guard expression.
 */
template<int X>
struct guard_placeholder {

    constexpr guard_placeholder() { }

    /**
     * @brief Convenient way to call <tt>gcall(fun, guard_placeholder)</tt>.
     */
    template<typename Fun>
    typename gcall1<Fun, guard_placeholder>::result operator()(Fun fun) const {
        return gcall(fun, *this);
    }

    // utility function for starts_with()
    static bool u8_starts_with(const std::string& lhs, const std::string& rhs) {
        return std::equal(rhs.begin(), rhs.end(), lhs.begin());
    }

    /**
     * @brief Evaluates to the size of a container.
     */
    typename gcall1<ge_get_size, guard_placeholder>::result size() const {
        return gcall(ge_get_size{}, *this);
    }

    typename gcall1<ge_is_empty, guard_placeholder>::result empty() const {
        return gcall(ge_is_empty{true}, *this);
    }

    typename gcall1<ge_is_empty, guard_placeholder>::result not_empty() const {
        return gcall(ge_is_empty{false}, *this);
    }

    /**
     * @brief Evaluates to the first element of a container if it's not empty.
     */
    typename gcall1<ge_get_front, guard_placeholder>::result front() const {
        return gcall(ge_get_front{}, *this);
    }

    /**
     * @brief Evaluates to true if unbound argument starts with @p str.
     */
    typename gcall2<decltype(&guard_placeholder::u8_starts_with),
                    guard_placeholder,
                    std::string
             >::result
    starts_with(std::string str) const {
        return gcall(&guard_placeholder::u8_starts_with, *this, std::move(str));
    }

    /**
     * @brief Evaluates to true if unbound argument
     *        is contained in @p container.
     */
    template<class C>
    typename gcall2<ge_search_container, C, guard_placeholder>::result
    in(C container) const {
        return gcall(ge_search_container{true}, std::move(container), *this);
    }

    /**
     * @brief Evaluates to true if unbound argument
     *        is contained in @p list.
     */
    template<typename T>
    typename gcall2<ge_search_container,
                     std::vector<typename detail::strip_and_convert<T>::type>,
                     guard_placeholder
             >::result
    in(std::initializer_list<T> list) const {
        std::vector<typename detail::strip_and_convert<T>::type> vec;
        for (auto& i : list) vec.emplace_back(i);
        return in(std::move(vec));
    }

    /**
     * @brief Evaluates to true if unbound argument
     *        is not contained in @p container.
     */
    template<class C>
    typename gcall2<ge_search_container, C, guard_placeholder>::result
    not_in(C container) const {
        return gcall(ge_search_container{false}, std::move(container), *this);
    }

    /**
     * @brief Evaluates to true if unbound argument
     *        is not contained in @p list.
     */
    template<typename T>
    typename gcall2<ge_search_container,
                     std::vector<typename detail::strip_and_convert<T>::type>,
                     guard_placeholder
             >::result
    not_in(std::initializer_list<T> list) const {
        std::vector<typename detail::strip_and_convert<T>::type> vec;
        for (auto& i : list) vec.emplace_back(i);
        return not_in(vec);
    }

};

// result type computation

template<typename T, class Tuple>
struct ge_unbound { typedef T type; };

template<typename T, class Tuple>
struct ge_unbound<ge_reference_wrapper<T>, Tuple> { typedef T type; };

template<typename T, class Tuple>
struct ge_unbound<std::reference_wrapper<T>, Tuple> { typedef T type; };

template<typename T, class Tuple>
struct ge_unbound<std::reference_wrapper<const T>, Tuple> { typedef T type; };

// unbound type of placeholder
template<int X, typename... Ts>
struct ge_unbound<guard_placeholder<X>, detail::tdata<Ts...> > {
    static_assert(X < sizeof...(Ts),
                  "Cannot unbind placeholder (too few arguments)");
    typedef typename ge_unbound<
                typename util::at<X, Ts...>::type,
                detail::tdata<std::reference_wrapper<Ts>...>
            >::type
            type;
};

// operators, operators, operators

template<typename T>
struct is_ge_type {
    static constexpr bool value = false;
};

template<int X>
struct is_ge_type<guard_placeholder<X> > {
    static constexpr bool value = true;
};

template<typename T>
struct is_ge_type<ge_reference_wrapper<T> > {
    static constexpr bool value = true;
};

template<operator_id OP, typename First, typename Second>
struct is_ge_type<guard_expr<OP, First, Second> > {
    static constexpr bool value = true;
};

template<operator_id OP, typename T1, typename T2>
guard_expr<OP, typename detail::strip_and_convert<T1>::type,
               typename detail::strip_and_convert<T2>::type>
ge_concatenate(T1 first, T2 second,
               typename std::enable_if<
                   is_ge_type<T1>::value || is_ge_type<T2>::value
               >::type* = 0) {
    return {first, second};
}

#define CPPA_GE_OPERATOR(EnumValue, Operator)                                  \
    template<typename T1, typename T2>                                         \
    auto operator Operator (T1 v1, T2 v2)                                      \
         -> decltype(ge_concatenate< EnumValue >(v1, v2)) {                    \
        return ge_concatenate< EnumValue >(v1, v2);                            \
    }

CPPA_FORALL_OPS(CPPA_GE_OPERATOR)

template<operator_id OP>
struct ge_eval_op;

#define CPPA_EVAL_OP_IMPL(EnumValue, Operator)                                 \
    template<> struct ge_eval_op< EnumValue > {                                \
        template<typename T1, typename T2>                                     \
        static inline auto _(const T1& lhs, const T2& rhs)                     \
        -> decltype(lhs Operator rhs) { return lhs Operator rhs; }             \
    };

CPPA_FORALL_OPS(CPPA_EVAL_OP_IMPL)
CPPA_EVAL_OP_IMPL(logical_and_op, &&)
CPPA_EVAL_OP_IMPL(logical_or_op, ||)

template<typename T, class Tuple>
struct ge_result_ {
    typedef typename ge_unbound<T, Tuple>::type type;
};

template<operator_id OP, typename First, typename Second, class Tuple>
struct ge_result_<guard_expr<OP, First, Second>, Tuple> {
    typedef typename ge_result_<First, Tuple>::type lhs_type;
    typedef typename ge_result_<Second, Tuple>::type rhs_type;
    typedef decltype(
            ge_eval_op<OP>::_(*static_cast<const lhs_type*>(nullptr),
                              *static_cast<const rhs_type*>(nullptr))) type;
};

template<typename Fun, class Tuple>
struct ge_result_<guard_expr<exec_xfun_op, Fun, util::void_type>, Tuple> {
    typedef bool type;
};

template<typename First, typename Second, class Tuple>
struct ge_result_<guard_expr<exec_fun1_op, First, Second>, Tuple> {
    typedef First type0;
    typedef typename ge_unbound<Second, Tuple>::type type1;
    typedef decltype( (*static_cast<const type0*>(nullptr))(
                 *static_cast<const type1*>(nullptr)
             )) type;
};

template<typename First, typename Second, class Tuple>
struct ge_result_<guard_expr<exec_fun2_op, First, Second>, Tuple> {
    typedef typename First::first_type type0;
    typedef typename ge_unbound<typename First::second_type, Tuple>::type type1;
    typedef typename ge_unbound<Second, Tuple>::type type2;
    typedef decltype( (*static_cast<const type0*>(nullptr))(
                 *static_cast<const type1*>(nullptr),
                 *static_cast<const type2*>(nullptr)
             )) type;
};

template<typename First, typename Second, class Tuple>
struct ge_result_<guard_expr<exec_fun3_op, First, Second>, Tuple> {
    typedef typename First::first_type type0;
    typedef typename ge_unbound<typename First::second_type,Tuple>::type type1;
    typedef typename ge_unbound<typename Second::first_type,Tuple>::type type2;
    typedef typename ge_unbound<typename Second::second_type,Tuple>::type type3;
    typedef decltype( (*static_cast<const type0*>(nullptr))(
                 *static_cast<const type1*>(nullptr),
                 *static_cast<const type2*>(nullptr),
                 *static_cast<const type3*>(nullptr)
             )) type;
};

template<operator_id OP, typename First, typename Second, class Tuple>
struct ge_result {
    typedef typename ge_result_<guard_expr<OP, First, Second>, Tuple>::type
            type;
};

template<operator_id OP1, typename F1, typename S1,
         operator_id OP2, typename F2, typename S2>
guard_expr<logical_and_op, guard_expr<OP1, F1, S1>, guard_expr<OP2, F2, S2>>
operator&&(guard_expr<OP1, F1, S1> lhs,
           guard_expr<OP2, F2, S2> rhs) {
    return {lhs, rhs};
}

template<operator_id OP1, typename F1, typename S1,
         operator_id OP2, typename F2, typename S2>
guard_expr<logical_or_op, guard_expr<OP1, F1, S1>, guard_expr<OP2, F2, S2>>
operator||(guard_expr<OP1, F1, S1> lhs,
           guard_expr<OP2, F2, S2> rhs) {
    return {lhs, rhs};
}


// evaluation of guard_expr

template<class Tuple, typename T>
inline const T& ge_resolve(const Tuple&, const T& value) {
    return value;
}

template<class Tuple, typename T>
inline const T& ge_resolve(const Tuple&, const std::reference_wrapper<T>& value) {
    return value.get();
}

template<class Tuple, typename T>
inline const T& ge_resolve(const Tuple&, const std::reference_wrapper<const T>& value) {
    return value.get();
}

template<class Tuple, typename T>
inline const T& ge_resolve(const Tuple&, const ge_reference_wrapper<T>& value) {
    return value.get();
}

template<class Tuple, int X>
inline auto ge_resolve(const Tuple& tup, guard_placeholder<X>)
       -> decltype(get<X>(tup).get()) {
    return get<X>(tup).get();
}

template<class Tuple, operator_id OP, typename First, typename Second>
auto ge_resolve(const Tuple& tup,
                const guard_expr<OP, First, Second>& ge)
     -> typename ge_result<OP, First, Second, Tuple>::type;

template<operator_id OP, class Tuple, typename First, typename Second>
struct ge_eval_ {
    static inline typename ge_result<OP, First, Second, Tuple>::type
    _(const Tuple& tup, const First& lhs, const Second& rhs) {
        return ge_eval_op<OP>::_(ge_resolve(tup, lhs), ge_resolve(tup, rhs));
    }
};

template<class Tuple, typename First, typename Second>
struct ge_eval_<logical_and_op, Tuple, First, Second> {
    static inline bool _(const Tuple& tup, const First& lhs, const Second& rhs) {
        // emulate short-circuit evaluation
        if (ge_resolve(tup, lhs)) return ge_resolve(tup, rhs);
        return false;
    }
};

template<class Tuple, typename First, typename Second>
struct ge_eval_<logical_or_op, Tuple, First, Second> {
    static inline bool _(const Tuple& tup, const First& lhs, const Second& rhs) {
        // emulate short-circuit evaluation
        if (ge_resolve(tup, lhs)) return true;
        return ge_resolve(tup, rhs);
    }
};

template<class Tuple, typename Fun>
struct ge_eval_<exec_xfun_op, Tuple, Fun, util::void_type> {
    static inline bool _(const Tuple& tup, const Fun& fun, const util::void_type&) {
        return util::unchecked_apply_tuple<bool>(fun, tup);
    }
};

template<class Tuple, typename First, typename Second>
struct ge_eval_<exec_fun1_op, Tuple, First, Second> {
    static inline auto _(const Tuple& tup, const First& fun, const Second& arg0)
         -> decltype(fun(ge_resolve(tup, arg0))) {
        return fun(ge_resolve(tup, arg0));
    }
};

template<class Tuple, typename First, typename Second>
struct ge_eval_<exec_fun2_op, Tuple, First, Second> {
    static inline auto _(const Tuple& tup, const First& lhs, const Second& rhs)
         -> decltype(lhs.m_args.first(ge_resolve(tup, lhs.m_args.second),
                                      ge_resolve(tup, rhs))) {
        return lhs.m_args.first(ge_resolve(tup, lhs.m_args.second),
                                ge_resolve(tup, rhs));
    }
};

template<class Tuple, typename First, typename Second>
struct ge_eval_<exec_fun3_op, Tuple, First, Second> {
    static inline auto _(const Tuple& tup, const First& lhs, const Second& rhs)
         -> decltype(lhs.m_args.first(ge_resolve(tup, lhs.m_args.second),
                                      ge_resolve(tup, rhs.m_args.first),
                                      ge_resolve(tup, rhs.m_args.second))) {
        return lhs.m_args.first(ge_resolve(tup, lhs.m_args.second),
                                ge_resolve(tup, rhs.m_args.first),
                                ge_resolve(tup, rhs.m_args.second));
    }
};

template<operator_id OP, class Tuple, typename First, typename Second>
inline typename ge_result<OP, First, Second, Tuple>::type
ge_eval(const Tuple& tup, const First& lhs, const Second& rhs) {
    return ge_eval_<OP, Tuple, First, Second>::_(tup, lhs, rhs);
}

template<class Tuple, operator_id OP, typename First, typename Second>
auto ge_resolve(const Tuple& tup,
                const guard_expr<OP, First, Second>& ge)
     -> typename ge_result<OP, First, Second, Tuple>::type {
    return ge_eval<OP>(tup, ge.m_args.first, ge.m_args.second);
}

template<operator_id OP, typename First, typename Second, typename... Args>
auto ge_invoke_step2(const guard_expr<OP, First, Second>& ge,
                     const detail::tdata<Args...>& tup)
     -> typename ge_result<OP, First, Second, detail::tdata<Args...>>::type {
    return ge_eval<OP>(tup, ge.m_args.first, ge.m_args.second);
}

template<operator_id OP, typename First, typename Second, typename... Args>
auto ge_invoke(const guard_expr<OP, First, Second>& ge,
               const Args&... args)
     -> typename ge_result<OP, First, Second,
                           detail::tdata<std::reference_wrapper<const Args>...>>::type {
    detail::tdata<std::reference_wrapper<const Args>...> tup{args...};
    return ge_invoke_step2(ge, tup);
}

template<class GuardExpr>
struct ge_invoke_helper {
    const GuardExpr& ge;
    ge_invoke_helper(const GuardExpr& arg) : ge(arg) { }
    template<typename... Args>
    bool operator()(Args&&... args) const {
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
ge_invoke_any(const guard_expr<OP, First, Second>& ge,
              const any_tuple& tup) {
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

template<operator_id OP, typename First, typename Second>
template<typename... Args>
bool guard_expr<OP, First, Second>::operator()(const Args&... args) const {
    static_assert(std::is_same<decltype(ge_invoke(*this, args...)), bool>::value,
                  "guard expression does not return a boolean");
    return ge_invoke(*this, args...);
}

// some utility functions

template<typename T>
struct gref_wrapped {
    typedef ge_reference_wrapper<typename util::rm_ref<T>::type> type;
};

template<typename T>
struct mutable_gref_wrapped {
    typedef ge_mutable_reference_wrapper<T> type;
};

template<typename T>
struct mutable_gref_wrapped<T&> {
    typedef ge_mutable_reference_wrapper<T> type;
};

// finally ...

namespace placeholders {

// doxygen cannot handle anonymous namespaces
#ifndef CPPA_DOCUMENTATION
namespace {
#endif // CPPA_DOCUMENTATION

constexpr guard_placeholder<0> _x1;
constexpr guard_placeholder<1> _x2;
constexpr guard_placeholder<2> _x3;
constexpr guard_placeholder<3> _x4;
constexpr guard_placeholder<4> _x5;
constexpr guard_placeholder<5> _x6;
constexpr guard_placeholder<6> _x7;
constexpr guard_placeholder<7> _x8;
constexpr guard_placeholder<8> _x9;

// doxygen cannot handle anonymous namespaces
#ifndef CPPA_DOCUMENTATION
} // namespace <anonymous>
#endif // CPPA_DOCUMENTATION

} // namespace placeholders

} // namespace cppa

#endif // CPPA_GUARD_EXPR_HPP
