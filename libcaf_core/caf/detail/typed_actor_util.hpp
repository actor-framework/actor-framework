/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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
    static constexpr size_t args = tl_size<arg_types>::value;
    static_assert(args <= tl_size<OutputList>::value,
                  "functor takes too much arguments");
    using rtypes = typename tl_right<OutputList, args>::type;
    static_assert(std::is_same<arg_types, rtypes>::value,
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

template <class A, class B, template <class, class> class Predicate>
struct static_asserter {
  static void verify_match() {
    static_assert(Predicate<A, B>::value, "exact match needed");
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

template <class Signatures, typename InputTypes>
struct deduce_output_type {
  static_assert(tl_find<
                  InputTypes,
                  atom_value
                >::value == -1,
                "atom(...) notation is not sufficient for static type "
                "checking, please use atom_constant instead in this context");
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

} // namespace detail
} // namespace caf

#endif // CAF_TYPED_ACTOR_DETAIL_HPP
