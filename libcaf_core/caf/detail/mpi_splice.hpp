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

#ifndef CAF_DETAIL_MPI_SPLICE_HPP
#define CAF_DETAIL_MPI_SPLICE_HPP

#include <type_traits>

#include "caf/replies_to.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/typed_actor_util.hpp"

namespace caf {
namespace detail {

template <class X, class... Ts>
struct mpi_splice_one;

template <class X>
struct mpi_splice_one<X> {
  using type = X;
};

template <class X, class... Ts>
struct mpi_splice_one<X, none_t, Ts...> {
  using type = none_t;
};

template <class... Xs, class... Ys, class... Zs, class... Ts>
struct mpi_splice_one<typed_mpi<type_list<Xs...>, type_list<Ys...>>,
                      typed_mpi<type_list<Xs...>, type_list<Zs...>>,
                      Ts...>
: mpi_splice_one<typed_mpi<type_list<Xs...>, type_list<Ys..., Zs...>>, Ts...> {
  // nop
};

template <template <class...> class Target, class List, class... Lists>
struct mpi_splice;

template <template <class...> class Target, class... Ts, class... Lists>
struct mpi_splice<Target, type_list<Ts...>, Lists...> {
  using spliced_list =
    type_list<
      typename mpi_splice_one<
        Ts,
        typename tl_find<
          Lists,
          input_is<typename Ts::input_types>::template eval
        >::type...
      >::type...
    >;
  using filtered_list =
    typename tl_filter_not_type<
      spliced_list,
      none_t
    >::type;
  static_assert(tl_size<filtered_list>::value > 0,
                "cannot splice incompatible actor handles");
  using type = typename tl_apply<filtered_list, Target>::type;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_MPI_SPLICE_HPP
