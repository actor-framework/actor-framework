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

#ifndef CPPA_TYPED_ACTOR_DETAIL_HPP
#define CPPA_TYPED_ACTOR_DETAIL_HPP

#include <tuple>

#include "cppa/replies_to.hpp"

#include "cppa/detail/type_list.hpp"

namespace cppa { // forward declarations

template<typename R>
class typed_continue_helper;

} // namespace cppa

namespace cppa {
namespace detail {

template<typename R, typename T>
struct deduce_signature_helper;

template<typename R, typename... Ts>
struct deduce_signature_helper<R, detail::type_list<Ts...>> {
    typedef typename replies_to<Ts...>::template with<R> type;

};

template<typename... Rs, typename... Ts>
struct deduce_signature_helper<std::tuple<Rs...>, detail::type_list<Ts...>> {
    typedef typename replies_to<Ts...>::template with<Rs...> type;

};

template<typename T>
struct deduce_signature {
    typedef typename detail::implicit_conversions<typename T::result_type>::type
    result_type;
    typedef typename detail::tl_map<typename T::arg_types,
                                    detail::rm_const_and_ref>::type arg_types;
    typedef typename deduce_signature_helper<result_type, arg_types>::type type;

};

template<typename Arguments>
struct input_is {
    template<typename Signature>
    struct eval {
        static constexpr bool value =
            std::is_same<Arguments, typename Signature::input_types>::value;

    };
};

template<typename OutputList, typename F>
inline void assert_types() {
    typedef typename detail::tl_map<
        typename detail::get_callable_trait<F>::arg_types,
        detail::rm_const_and_ref>::type arg_types;
    static constexpr size_t fun_args = detail::tl_size<arg_types>::value;
    static_assert(fun_args <= detail::tl_size<OutputList>::value,
                  "functor takes too much arguments");
    typedef typename detail::tl_right<OutputList, fun_args>::type recv_types;
    static_assert(std::is_same<arg_types, recv_types>::value,
                  "wrong functor signature");
}

template<typename T>
struct lifted_result_type {
    typedef detail::type_list<typename detail::implicit_conversions<T>::type>
    type;

};

template<typename... Ts>
struct lifted_result_type<std::tuple<Ts...>> {
    typedef detail::type_list<Ts...> type;

};

template<typename T>
struct deduce_output_type_step2 {
    typedef T type;

};

template<typename R>
struct deduce_output_type_step2<detail::type_list<typed_continue_helper<R>>> {
    typedef typename lifted_result_type<R>::type type;

};

template<typename Signatures, typename InputTypes>
struct deduce_output_type {
    static constexpr int input_pos = detail::tl_find_if<
        Signatures, input_is<InputTypes>::template eval>::value;
    static_assert(input_pos >= 0, "typed actor does not support given input");
    typedef typename detail::tl_at<Signatures, input_pos>::type signature;
    typedef typename deduce_output_type_step2<
        typename signature::output_types>::type type;

};

} // namespace detail
} // namespace cppa

#endif // CPPA_TYPED_ACTOR_DETAIL_HPP
