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

#ifndef CAF_DETAIL_CTM_HPP
#define CAF_DETAIL_CTM_HPP

#include "caf/replies_to.hpp"
#include "caf/typed_response_promise.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/typed_actor_util.hpp"

namespace caf {
namespace detail {

// CTM: Compile-Time Match

// left hand side is the MPI we are comparing to, this is *not* commutative
template <class A, class B>
struct ctm_cmp : std::false_type { };

template <class In, class L, class R1, class R2>
struct ctm_cmp<typed_mpi<In, L, R1>,
               typed_mpi<In, L, R2>> {
  static constexpr bool value = std::is_same<R1, R2>::value
                                || std::is_same<R2, empty_type_list>::value;
};


/*
template <class In, class Out>
struct ctm_cmp<typed_mpi<In, Out, empty_type_list>,
               typed_mpi<In, Out, empty_type_list>>
    : std::true_type { };

template <class In, class L, class R>
struct ctm_cmp<typed_mpi<In, L, R>,
               typed_mpi<In, L, R>>
    : std::true_type { };
*/

template <class In, class Out>
struct ctm_cmp<typed_mpi<In, Out, empty_type_list>,
               typed_mpi<In, type_list<typed_continue_helper<Out>>, empty_type_list>>
    : std::true_type { };

template <class In, class... Ts>
struct ctm_cmp<typed_mpi<In, type_list<Ts...>, empty_type_list>,
               typed_mpi<In, type_list<typed_response_promise<Ts...>>, empty_type_list>>
    : std::true_type { };

template <class In, class L, class R>
struct ctm_cmp<typed_mpi<In, L, R>,
               typed_mpi<In, type_list<skip_message_t>, empty_type_list>>
    : std::true_type { };

template <class In, class L, class R>
struct ctm_cmp<typed_mpi<In, L, R>,
               typed_mpi<In, type_list<typed_response_promise<either_or_t<L, R>>>, empty_type_list>>
    : std::true_type { };

/*
template <class In, class L, class R>
struct ctm_cmp<typed_mpi<In, L, R>,
               typed_mpi<In, L, empty_type_list>>
    : std::true_type { };
*/

template <class In, class L, class R>
struct ctm_cmp<typed_mpi<In, L, R>,
               typed_mpi<In, R, empty_type_list>>
    : std::true_type { };

template <class A, class B, int Pos = 0>
struct ctm_impl { // : std::integral_constant<int, -2> {
  // -2 means: to few message handlers defined
  static constexpr int value = (tl_size<A>::value < tl_size<B>::value)
                               ? -2
                               : -3;
};

template <int Pos>
struct ctm_impl<empty_type_list, empty_type_list, Pos>
    : std::integral_constant<int, -1> {
  // everything's fine, -1 means: no mismatch found (both sets are empty)
};

template <class X, class... Xs, class... Ys, int Pos>
struct ctm_impl<type_list<X, Xs...>, type_list<Ys...>, Pos> {
  using next_ys =
    typename tl_filter_not<
      type_list<Ys...>,
      tbind<ctm_cmp, X>::template type
    >::type;
  static constexpr int value =
    // check if filter_not did remove something
    sizeof...(Ys) == tl_size<next_ys>::value
    ? Pos // error at this position
    : ctm_impl<type_list<Xs...>, next_ys, Pos + 1>::value;
};

template <class X, class... Xs, class... Ys, int Pos>
constexpr int ctm_impl<type_list<X, Xs...>, type_list<Ys...>, Pos>::value;

template <class X, class Y>
struct ctm {
  // -3 means too many handler, -2 means too few, -1 means OK, everything else
  // mismatch at that position
  static constexpr size_t num_xs = tl_size<X>::value;
  static constexpr size_t num_ys = tl_size<Y>::value;
  /*
  static constexpr int value = num_xs != num_ys
                               ? num_xs > num_ys ? -3 : -2
                               : ctm_impl<X, Y, 0>::value;
  */
  static constexpr int value = ctm_impl<X, Y, 0>::value;
};

template <class X, class Y>
constexpr int ctm<X,Y>::value;

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_CTM_HPP
