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

#ifndef CAF_TYPED_ACTOR_DETAIL_HPP
#define CAF_TYPED_ACTOR_DETAIL_HPP

#include <tuple>

#include "caf/replies_to.hpp"
#include "caf/system_messages.hpp"

#include "caf/detail/type_list.hpp"

namespace caf { // forward declarations

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

template <class L, class R>
struct deduce_lhs_result<either_or_t<L, R>> {
  using type = L;
};

template <class T>
struct deduce_rhs_result {
  using type = type_list<>;
};

template <class L, class R>
struct deduce_rhs_result<either_or_t<L, R>> {
  using type = R;
};

template <class T>
struct is_hidden_msg_handler : std::false_type { };

template <>
struct is_hidden_msg_handler<typed_mpi<type_list<exit_msg>,
                                       type_list<void>,
                                       empty_type_list>> : std::true_type { };

template <>
struct is_hidden_msg_handler<typed_mpi<type_list<down_msg>,
                                       type_list<void>,
                                       empty_type_list>> : std::true_type { };

template <class T>
struct deduce_mpi {
  using result = typename implicit_conversions<typename T::result_type>::type;
  using arg_t = typename tl_map<typename T::arg_types, std::decay>::type;
  using type = typed_mpi<arg_t,
                         typename deduce_lhs_result<result>::type,
                         typename deduce_rhs_result<result>::type>;
};

template <class Arguments>
struct input_is {
  template <class Signature>
  struct eval {
    static constexpr bool value =
      std::is_same<Arguments, typename Signature::input_types>::value;
  };
};

template <class OutputPair, class... Fs>
struct type_checker;

template <class OutputList, class F1>
struct type_checker<OutputList, F1> {
  static void check() {
    using arg_types =
      typename tl_map<
        typename get_callable_trait<F1>::arg_types,
        std::decay
      >::type;
    static_assert(std::is_same<OutputList, arg_types>::value
                  || (std::is_same<OutputList, type_list<void>>::value
                     && std::is_same<arg_types, type_list<>>::value),
                  "wrong functor signature");
  }
};

template <class Opt1, class Opt2, class F1>
struct type_checker<type_pair<Opt1, Opt2>, F1> {
  static void check() {
    type_checker<Opt1, F1>::check();
  }
};

template <class OutputPair, class F1>
struct type_checker<OutputPair, F1, none_t> : type_checker<OutputPair, F1> { };

template <class Opt1, class Opt2, class F1, class F2>
struct type_checker<type_pair<Opt1, Opt2>, F1, F2> {
  static void check() {
    type_checker<Opt1, F1>::check();
    type_checker<Opt2, F2>::check();
  }
};

template <int X, int Pos>
struct static_error_printer {
  static_assert(X != Pos, "unexpected handler some position > 20");

};

template <int X>
struct static_error_printer<X, -3> {
  static_assert(X == -1, "too few message handlers defined");
};

template <int X>
struct static_error_printer<X, -2> {
  static_assert(X == -1, "too many message handlers defined");
};

template <int X>
struct static_error_printer<X, -1> {
  // everything' fine
};

#define CAF_STATICERR(Pos)                                                     \
  template <int X>                                                             \
  struct static_error_printer< X, Pos > {                                      \
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
    static_error_printer<x, x> dummy;
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
struct deduce_output_type {
  static constexpr int input_pos =
    tl_find_if<
      Signatures,
      input_is<InputTypes>::template eval
    >::value;
  static_assert(input_pos != -1, "typed actor does not support given input");
  using signature = typename tl_at<Signatures, input_pos>::type;
  using type = detail::type_pair<typename signature::output_opt1_types,
                                 typename signature::output_opt2_types>;
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
    sender_signature_checker<
      DestSigs, OrigSigs,
      typename dest_output_types::first
    >::check();
    sender_signature_checker<
      DestSigs, OrigSigs,
      typename dest_output_types::second
    >::check();
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

} // namespace detail
} // namespace caf

#endif // CAF_TYPED_ACTOR_DETAIL_HPP
