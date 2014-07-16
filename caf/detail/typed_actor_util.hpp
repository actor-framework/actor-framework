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

#ifndef CAF_TYPED_ACTOR_DETAIL_HPP
#define CAF_TYPED_ACTOR_DETAIL_HPP

#include <tuple>

#include "caf/replies_to.hpp"

#include "caf/detail/type_list.hpp"

namespace caf { // forward declarations

template<typename R>
class typed_continue_helper;

} // namespace caf

namespace caf {
namespace detail {

template<typename R, typename T>
struct deduce_signature_helper;

template<typename R, typename... Ts>
struct deduce_signature_helper<R, type_list<Ts...>> {
    using type = typename replies_to<Ts...>::template with<R>;

};

template<typename... Rs, typename... Ts>
struct deduce_signature_helper<std::tuple<Rs...>, type_list<Ts...>> {
    using type = typename replies_to<Ts...>::template with<Rs...>;

};

template<typename T>
struct deduce_signature {

    using result_type = typename implicit_conversions<
                            typename T::result_type
                        >::type;

    using arg_types = typename tl_map<
                          typename T::arg_types,
                          rm_const_and_ref
                      >::type;

    using type = typename deduce_signature_helper<result_type, arg_types>::type;

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

    using arg_types = typename tl_map<
                          typename get_callable_trait<F>::arg_types,
                          rm_const_and_ref
                      >::type;

    static constexpr size_t fun_args = tl_size<arg_types>::value;

    static_assert(fun_args <= tl_size<OutputList>::value,
                  "functor takes too much arguments");

    using recv_types = typename tl_right<OutputList, fun_args>::type;

    static_assert(std::is_same<arg_types, recv_types>::value,
                  "wrong functor signature");

}

template<typename T>
struct lifted_result_type {
    using type = type_list<typename implicit_conversions<T>::type>;
};

template<typename... Ts>
struct lifted_result_type<std::tuple<Ts...>> {
    using type = type_list<Ts...>;
};

template<typename T>
struct deduce_output_type_step2 {
    using type = T;
};

template<typename R>
struct deduce_output_type_step2<type_list<typed_continue_helper<R>>> {
    using type = typename lifted_result_type<R>::type;
};

template<typename Signatures, typename InputTypes>
struct deduce_output_type {
    static constexpr int input_pos = tl_find_if<
                                         Signatures,
                                         input_is<InputTypes>::template eval
                                     >::value;

    static_assert(input_pos >= 0, "typed actor does not support given input");

    using signature = typename tl_at<Signatures, input_pos>::type;

    using type = typename deduce_output_type_step2<
                     typename signature::output_types
                 >::type;

};

} // namespace detail
} // namespace caf

#endif // CAF_TYPED_ACTOR_DETAIL_HPP
