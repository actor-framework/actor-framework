/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_DETAIL_TYPED_ACTOR_DETAIL_HPP
#define CAF_DETAIL_TYPED_ACTOR_DETAIL_HPP

#include <tuple>

#include "caf/delegated.hpp"
#include "caf/replies_to.hpp"
#include "caf/system_messages.hpp"

#include "caf/detail/type_list.hpp"

namespace caf { // forward declarations

template <class... Ts>
class typed_actor;

template <class R>
class typed_continue_helper;

} // namespace caf

namespace caf {
namespace detail {

template <class T>
struct unwrap_std_tuple {
  using type = type_list<T>;
};

template <class... Ts>
struct unwrap_std_tuple<std::tuple<Ts...>> {
  using type = type_list<Ts...>;
};

template <class T>
struct deduce_lhs_result {
  using type = typename unwrap_std_tuple<T>::type;
};

template <class T>
struct deduce_rhs_result {
  using type = type_list<>;
};

template <class T>
struct deduce_mpi {
  using result = typename implicit_conversions<typename T::result_type>::type;
  using arg_t = typename tl_map<typename T::arg_types, std::decay>::type;
  using type = typed_mpi<arg_t,
                         typename deduce_lhs_result<result>::type>;
};

template <class Arguments, class Signature>
struct input_is_eval_impl : std::false_type {};

template <class Arguments, class Out>
struct input_is_eval_impl<Arguments, typed_mpi<Arguments, Out>> : std::true_type {};

template <class Arguments>
struct input_is {
  template <class Signature>
  struct eval : input_is_eval_impl<Arguments, Signature> { };
};

template <class Output, class F>
struct type_checker {
  static void check() {
    using arg_types =
      typename tl_map<
        typename get_callable_trait<F>::arg_types,
        std::decay
      >::type;
    static_assert(std::is_same<Output, arg_types>::value
                  || (std::is_same<Output, type_list<void>>::value
                     && std::is_same<arg_types, type_list<>>::value),
                  "wrong functor signature");
  }
};


template <class F>
struct type_checker<message, F> {
  static void check() {
    // nop
  }
};

template <int X, int Pos, class A>
struct static_error_printer {
  static_assert(X != Pos, "unexpected handler some position > 20");
};

template <int X, class A>
struct static_error_printer<X, -3, A> {
  static_assert(X == -1, "too few message handlers defined");
};

template <int X, class A>
struct static_error_printer<X, -2, A> {
  static_assert(X == -1, "too many message handlers defined");
};

template <int X, class A>
struct static_error_printer<X, -1, A> {
  // everything' fine
};

#define CAF_STATICERR(Pos)                                                     \
  template <int X, class A>                                                    \
  struct static_error_printer< X, Pos, A > {                                   \
    static_assert(X == -1, "unexpected handler at position " #Pos );           \
  }

CAF_STATICERR( 0); CAF_STATICERR( 1); CAF_STATICERR( 2);
CAF_STATICERR( 3); CAF_STATICERR( 4); CAF_STATICERR( 5);
CAF_STATICERR( 6); CAF_STATICERR( 7); CAF_STATICERR( 8);
CAF_STATICERR( 9); CAF_STATICERR(10); CAF_STATICERR(11);
CAF_STATICERR(12); CAF_STATICERR(13); CAF_STATICERR(14);
CAF_STATICERR(15); CAF_STATICERR(16); CAF_STATICERR(17);
CAF_STATICERR(18); CAF_STATICERR(19); CAF_STATICERR(20);

template <class A, class B, template <class, class> class Predicate>
struct static_asserter {
  static void verify_match() {
    static constexpr int x = Predicate<A, B>::value;
    using type_x = typename tl_at<B, (x < 0 ? 0 : x)>::type;
    static_error_printer<x, x, type_x> dummy;
    static_cast<void>(dummy);
  }
};

template <class T>
struct lifted_result_type {
  using type = type_list<typename implicit_conversions<T>::type>;
};

template <class... Ts>
struct lifted_result_type<std::tuple<Ts...>> {
  using type = type_list<Ts...>;
};

template <class T>
struct deduce_lifted_output_type {
  using type = T;
};

template <class R>
struct deduce_lifted_output_type<type_list<typed_continue_helper<R>>> {
  using type = typename lifted_result_type<R>::type;
};

template <class Signatures, class InputTypes>
struct deduce_output_type_impl {
  using signature =
    typename tl_find<
      Signatures,
      input_is<InputTypes>::template eval
    >::type;
  static_assert(! std::is_same<signature, none_t>::value,
                "typed actor does not support given input");
  using type = typename signature::output_types;
  // generates the appropriate `delegated<...>` type from given signatures
  using delegated_type = typename detail::tl_apply<type, delegated>::type;
  // generates the appropriate `std::tuple<...>` type from given signature
  using tuple_type = typename detail::tl_apply<type, std::tuple>::type;
};

template <class InputTypes>
struct deduce_output_type_impl<none_t, InputTypes> {
  using type = message;
  using delegated_type = delegated<message>;
  using tuple_type = std::tuple<message>;
};

template <class Handle, class InputTypes>
struct deduce_output_type
    : deduce_output_type_impl<typename Handle::signatures, InputTypes> {
  // nop
};

template <class T, class InputTypes>
struct deduce_output_type<T*, InputTypes>
  : deduce_output_type_impl<typename T::signatures, InputTypes> {
  // nop
};

template <class... Ts>
struct common_result_type;

template <class T>
struct common_result_type<T> {
  using type = T;
};

template <class T, class... Us>
struct common_result_type<T, T, Us...> {
  using type = typename common_result_type<T, Us...>::type;
};

template <class T1, class T2, class... Us>
struct common_result_type<T1, T2, Us...> {
  using type = void;
};

template <class OrigSigs, class DestSigs, class ArgTypes>
struct sender_signature_checker {
  static void check() {
    using dest_output_types =
      typename deduce_output_type<
        DestSigs, ArgTypes
      >::type;
    sender_signature_checker<DestSigs, OrigSigs, dest_output_types>::check();
  }
};

template <class OrigSigs, class DestSigs>
struct sender_signature_checker<OrigSigs, DestSigs, detail::type_list<void>> {
  static void check() {}
};

template <class OrigSigs, class DestSigs>
struct sender_signature_checker<OrigSigs, DestSigs, detail::type_list<>> {
  static void check() {}
};

template <class... Ts>
struct extend_with_helper;

template <class... Xs>
struct extend_with_helper<typed_actor<Xs...>> {
  using type = typed_actor<Xs...>;
};

template <class... Xs, class... Ys, class... Ts>
struct extend_with_helper<typed_actor<Xs...>, typed_actor<Ys...>, Ts...>
  : extend_with_helper<typed_actor<Xs..., Ys...>, Ts...> {
  // nop
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_TYPED_ACTOR_DETAIL_HPP
