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

#ifndef CPPA_DETAIL_TYPE_TRAITS_HPP
#define CPPA_DETAIL_TYPE_TRAITS_HPP

#include <string>
#include <functional>
#include <type_traits>

#include "cppa/optional.hpp"

#include "cppa/fwd.hpp"

#include "cppa/detail/type_list.hpp"

namespace cppa {
namespace detail {

/**
 * @addtogroup MetaProgramming
 * @{
 */

/**
 * @brief Equal to std::remove_const<std::remove_reference<T>::type>.
 */
template<typename T>
struct rm_const_and_ref {
    using type = T;

};

template<typename T>
struct rm_const_and_ref<const T&> {
    using type = T;

};

template<typename T>
struct rm_const_and_ref<const T> {
    using type = T;

};

template<typename T>
struct rm_const_and_ref<T&> {
    using type = T;

};

// template<> struct rm_const_and_ref<void> { };

/**
 * @brief Joins all bool constants using operator &&.
 */
template<bool... BoolConstants>
struct conjunction;

template<>
struct conjunction<> {
    static constexpr bool value = false;

};

template<bool V0>
struct conjunction<V0> {
    static constexpr bool value = V0;

};

template<bool V0, bool V1, bool... Vs>
struct conjunction<V0, V1, Vs...> {
    static constexpr bool value = V0 && conjunction<V1, Vs...>::value;

};

/**
 * @brief Joins all bool constants using operator ||.
 */
template<bool... BoolConstants>
struct disjunction;

template<bool V0, bool... Vs>
struct disjunction<V0, Vs...> {
    static constexpr bool value = V0 || disjunction<Vs...>::value;

};

template<>
struct disjunction<> {
    static constexpr bool value = false;

};

/**
 * @brief Equal to std::is_same<T, anything>.
 */
template<typename T>
struct is_anything : std::is_same<T, anything> {};

/**
 * @brief Checks whether @p T is an array of type @p U.
 */
template<typename T, typename U>
struct is_array_of {
    using step1_type = typename std::remove_all_extents<T>::type;
    using step2_type = typename std::remove_cv<step1_type>::type;
    static constexpr bool value =
        std::is_array<T>::value && std::is_same<step2_type, U>::value;

};

/**
 * @brief Deduces the reference type of T0 and applies it to T1.
 */
template<typename T0, typename T1>
struct deduce_ref_type {
    using type = typename detail::rm_const_and_ref<T1>::type;

};

template<typename T0, typename T1>
struct deduce_ref_type<T0&, T1> {
    using type = typename detail::rm_const_and_ref<T1>::type&;

};

template<typename T0, typename T1>
struct deduce_ref_type<const T0&, T1> {
    using type = const typename detail::rm_const_and_ref<T1>::type&;

};

/**
 * @brief Checks wheter @p X is in the template parameter pack Ts.
 */
template<typename X, typename... Ts>
struct is_one_of;

template<typename X>
struct is_one_of<X> : std::false_type {};

template<typename X, typename... Ts>
struct is_one_of<X, X, Ts...> : std::true_type {};

template<typename X, typename T0, typename... Ts>
struct is_one_of<X, T0, Ts...> : is_one_of<X, Ts...> {};

/**
 * @brief Checks wheter @p T is considered a builtin type.
 *
 * Builtin types are: (1) all arithmetic types, (2) string types from the STL,
 * and (3) built-in types such as @p actor_ptr.
 */
template<typename T>
struct is_builtin {
    // all arithmetic types are considered builtin
    static constexpr bool value =
        std::is_arithmetic<T>::value ||
        is_one_of<T, anything, std::string, std::u16string, std::u32string,
                  atom_value, message, actor, group, channel, node_id>::value;

};

/**
 * @brief Chekcs wheter @p T is primitive, i.e., either an arithmetic
 *        type or convertible to one of STL's string types.
 */
template<typename T>
struct is_primitive {
    static constexpr bool value =
        std::is_arithmetic<T>::value ||
        std::is_convertible<T, std::string>::value ||
        std::is_convertible<T, std::u16string>::value ||
        std::is_convertible<T, std::u32string>::value ||
        std::is_convertible<T, atom_value>::value;

};

/**
 * @brief Chekcs wheter @p T1 is comparable with @p T2.
 */
template<typename T1, typename T2>
class is_comparable {

    // SFINAE: If you pass a "bool*" as third argument, then
    //         decltype(cmp_help_fun(...)) is bool if there's an
    //         operator==(A, B) because
    //         cmp_help_fun(A*, B*, bool*) is a better match than
    //         cmp_help_fun(A*, B*, void*). If there's no operator==(A, B)
    //         available, then cmp_help_fun(A*, B*, void*) is the only
    //         candidate and thus decltype(cmp_help_fun(...)) is void.

    template<typename A, typename B>
    static bool cmp_help_fun(const A* arg0, const B* arg1,
                             decltype(*arg0 == *arg1)* = nullptr) {
        return true;
    }

    template<typename A, typename B>
    static void cmp_help_fun(const A*, const B*, void* = nullptr) {}

    using result_type = decltype(cmp_help_fun(static_cast<T1*>(nullptr),
                                              static_cast<T2*>(nullptr),
                                              static_cast<bool*>(nullptr)));

 public:

    static constexpr bool value = std::is_same<bool, result_type>::value;

};

/**
 * @brief Checks wheter @p T behaves like a forward iterator.
 */
template<typename T>
class is_forward_iterator {

    template<class C>
    static bool sfinae_fun(
        C* iter,
        // check for 'C::value_type C::operator*()' returning a non-void type
        typename rm_const_and_ref<decltype(*(*iter))>::type* = 0,
        // check for 'C& C::operator++()'
        typename std::enable_if<
            std::is_same<C&, decltype(++(*iter))>::value>::type* = 0,
        // check for 'bool C::operator==()'
        typename std::enable_if<
            std::is_same<bool, decltype(*iter == *iter)>::value>::type* = 0,
        // check for 'bool C::operator!=()'
        typename std::enable_if<
            std::is_same<bool, decltype(*iter != *iter)>::value>::type* = 0) {
        return true;
    }

    static void sfinae_fun(void*) {}

    using result_type = decltype(sfinae_fun(static_cast<T*>(nullptr)));

 public:

    static constexpr bool value = std::is_same<bool, result_type>::value;

};

/**
 * @brief Checks wheter @p T has <tt>begin()</tt> and <tt>end()</tt> member
 *        functions returning forward iterators.
 */
template<typename T>
class is_iterable {

    // this horrible code would just disappear if we had concepts
    template<class C>
    static bool sfinae_fun(
        const C* cc,
        // check for 'C::begin()' returning a forward iterator
        typename std::enable_if<
            detail::is_forward_iterator<decltype(cc->begin())>::value>::type* =
            0,
        // check for 'C::end()' returning the same kind of forward iterator
        typename std::enable_if<std::is_same<
            decltype(cc->begin()), decltype(cc->end())>::value>::type* = 0) {
        return true;
    }

    // SFNINAE default
    static void sfinae_fun(const void*) {}

    using result_type = decltype(sfinae_fun(static_cast<const T*>(nullptr)));

 public:

    static constexpr bool value = detail::is_primitive<T>::value == false &&
                                  std::is_same<bool, result_type>::value;

};

/**
 * @brief Checks wheter @p T is neither a reference nor a pointer nor an array.
 */
template<typename T>
struct is_legal_tuple_type {
    static constexpr bool value = std::is_reference<T>::value == false &&
                                  std::is_pointer<T>::value == false &&
                                  std::is_array<T>::value == false;

};

/**
 * @brief Checks wheter @p T is a non-const reference.
 */
template<typename T>
struct is_mutable_ref {
    static constexpr bool value =
        std::is_reference<T>::value && not std::is_const<T>::value;

};

/**
 * @brief Returns either @p T or @p T::type if @p T is an option.
 */
template<typename T>
struct rm_optional {
    using type = T;

};

template<typename T>
struct rm_optional<optional<T>> {
    using type = T;

};

/**
 * @brief Defines @p result_type, @p arg_types, and @p fun_type. Functor is
 *        (a) a member function pointer, (b) a function,
 *        (c) a function pointer, (d) an std::function.
 *
 * @p result_type is the result type found in the signature.
 * @p arg_types are the argument types as {@link type_list}.
 * @p fun_type is an std::function with an equivalent signature.
 */
template<typename Functor>
struct callable_trait;

// member const function pointer
template<class C, typename Result, typename... Ts>
struct callable_trait<Result (C::*)(Ts...) const> {
    using result_type = Result;
    using arg_types = type_list<Ts...>;
    using fun_type = std::function<Result(Ts...)>;

};

// member function pointer
template<class C, typename Result, typename... Ts>
struct callable_trait<Result (C::*)(Ts...)> {
    using result_type = Result;
    using arg_types = type_list<Ts...>;
    using fun_type = std::function<Result(Ts...)>;

};

// good ol' function
template<typename Result, typename... Ts>
struct callable_trait<Result(Ts...)> {
    using result_type = Result;
    using arg_types = type_list<Ts...>;
    using fun_type = std::function<Result(Ts...)>;

};

// good ol' function pointer
template<typename Result, typename... Ts>
struct callable_trait<Result (*)(Ts...)> {
    using result_type = Result;
    using arg_types = type_list<Ts...>;
    using fun_type = std::function<Result(Ts...)>;

};

// matches (IsFun || IsMemberFun)
template<bool IsFun, bool IsMemberFun, typename T>
struct get_callable_trait_helper {
    using type = callable_trait<T>;

};

// assume functor providing operator()
template<typename C>
struct get_callable_trait_helper<false, false, C> {
    using type = callable_trait<decltype(&C::operator())>;

};

/**
 * @brief Gets a callable trait for @p T, where @p T is a functor type,
 *        i.e., a function, member function, or a class providing
 *        the call operator.
 */
template<typename T>
struct get_callable_trait {
    // type without cv qualifiers
    using bare_type = typename rm_const_and_ref<T>::type;
    // if T is a function pointer, this type identifies the function
    using signature_type = typename std::remove_pointer<bare_type>::type;
    using type = typename get_callable_trait_helper<
                        std::is_function<bare_type>::value
                     || std::is_function<signature_type>::value,
                     std::is_member_function_pointer<bare_type>::value,
                     bare_type
                 >::type;
    using result_type = typename type::result_type;
    using arg_types = typename type::arg_types;
    using fun_type = typename type::fun_type;
};

/**
 * @brief Checks wheter @p T is a function or member function.
 */
template<typename T>
struct is_callable {

    template<typename C>
    static bool _fun(C*, typename callable_trait<C>::result_type* = nullptr) {
        return true;
    }

    template<typename C>
    static bool
    _fun(C*,
         typename callable_trait<decltype(&C::operator())>::result_type* = 0) {
        return true;
    }

    static void _fun(void*) { }

    using pointer = typename rm_const_and_ref<T>::type*;

    using result_type = decltype(_fun(static_cast<pointer>(nullptr)));

 public:

    static constexpr bool value = std::is_same<bool, result_type>::value;

};

/**
 * @brief Checks wheter each @p T in @p Ts is a function or member function.
 */
template<typename... Ts>
struct all_callable {
    static constexpr bool value = conjunction<is_callable<Ts>::value...>::value;

};

/**
 * @brief Checks wheter @p F takes mutable references.
 *
 * A manipulator is a functor that manipulates its arguments via
 * mutable references.
 */
template<typename F>
struct is_manipulator {
    static constexpr bool value = tl_exists<
        typename get_callable_trait<F>::arg_types, is_mutable_ref>::value;

};

template<bool IsCallable, typename C>
struct map_to_result_type_impl {
    using trait_type = typename get_callable_trait<C>::type;
    using type = typename trait_type::result_type;

};

template<typename C>
struct map_to_result_type_impl<false, C> {
    using type = unit_t;

};

/**
 * @brief Maps @p T to its result type if it's callable,
 *        {@link unit_t} otherwise.
 */
template<typename T>
struct map_to_result_type {
    using type = typename map_to_result_type_impl<
                     is_callable<T>::value,
                     T
                 >::type;
};

template<bool DoReplace, typename T1, typename T2>
struct replace_type_impl {
    using type = T1;
};

template<typename T1, typename T2>
struct replace_type_impl<true, T1, T2> {
    using type = T2;
};

/**
 * @brief Replaces @p What with @p With if any IfStmt::value evaluates to true.
 */
template<typename What, typename With, typename... IfStmt>
struct replace_type {
    static constexpr bool do_replace = disjunction<IfStmt::value...>::value;
    using type = typename replace_type_impl<do_replace, What, With>::type;
};

/**
 * @brief Gets the Nth element of the template parameter pack @p Ts.
 */
template<size_t N, typename... Ts>
struct type_at;

template<size_t N, typename T0, typename... Ts>
struct type_at<N, T0, Ts...> {
    using type = typename type_at<N - 1, Ts...>::type;
};

template<typename T0, typename... Ts>
struct type_at<0, T0, Ts...> {
    using type = T0;
};

/**
 * @}
 */

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_TYPE_TRAITS_HPP
