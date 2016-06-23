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

#include <tuple>

#include "caf/optional.hpp"
#include "caf/delegated.hpp"
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

template <class In, class Out>
struct ctm_cmp<typed_mpi<In, Out>,
               typed_mpi<In, Out>> {
  static constexpr bool value = true;
};

template <class In, class OutList>
struct ctm_cmp<typed_mpi<In, OutList>,
               typed_mpi<In, type_list<typed_continue_helper<OutList>>>>
    : std::true_type { };

template <class In, class Out>
struct ctm_cmp<typed_mpi<In, type_list<Out>>,
               typed_mpi<In, type_list<optional<Out>>>>
    : std::true_type { };

template <class In, class... Ts>
struct ctm_cmp<typed_mpi<In, type_list<Ts...>>,
               typed_mpi<In, type_list<typed_response_promise<Ts...>>>>
    : std::true_type { };

template <class In, class... Ts>
struct ctm_cmp<typed_mpi<In, type_list<Ts...>>,
               typed_mpi<In, type_list<optional<std::tuple<Ts...>>>>>
    : std::true_type { };

template <class In, class... Ts>
struct ctm_cmp<typed_mpi<In, type_list<Ts...>>,
               typed_mpi<In, type_list<result<Ts...>>>>
    : std::true_type { };

template <class In, class T>
struct ctm_cmp<typed_mpi<In, type_list<T>>,
               typed_mpi<In, type_list<expected<T>>>>
    : std::true_type { };

template <class In, class Out>
struct ctm_cmp<typed_mpi<In, Out>,
               typed_mpi<In, type_list<skip_t>>>
    : std::true_type { };

template <class In, class... Ts>
struct ctm_cmp<typed_mpi<In, type_list<Ts...>>,
               typed_mpi<In, type_list<delegated<Ts...>>>>
    : std::true_type { };

template <class Xs, class Ys>
constexpr int ctm_impl(int pos) {
  return tl_empty<Xs>::value
      ? -1 // consumed each X
      : (tl_exists<Ys, tbind<ctm_cmp, typename tl_head<Xs>::type>::template type>::value
         ? ctm_impl<typename tl_tail<Xs>::type, Ys>(pos + 1)
         : pos);
}

template <class Xs, class Ys>
struct ctm {
  // -3 means too many handler, -2 means too few, -1 means OK, everything else
  // mismatch at that position
  static constexpr size_t num_xs = tl_size<Xs>::value;
  static constexpr size_t num_ys = tl_size<Ys>::value;
  static constexpr int value = num_xs == num_ys ? ctm_impl<Xs, Ys>(0)
                                                : (num_xs < num_ys ? -2 : -3);
};

template <class Xs, class Ys>
constexpr int ctm<Xs, Ys>::value;

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_CTM_HPP
