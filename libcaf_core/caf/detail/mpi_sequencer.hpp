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

#ifndef CAF_DETAIL_MPI_SEQUENCER_HPP
#define CAF_DETAIL_MPI_SEQUENCER_HPP

#include "caf/replies_to.hpp"

#include "caf/detail/type_list.hpp"

namespace caf {
namespace detail {

template <class X, class Y>
struct mpi_sequencer_one {
  using type = void;
};

template <class... Xs, class... Ys, class... Zs>
struct mpi_sequencer_one<typed_mpi<type_list<Xs...>, type_list<Ys...>>,
                         typed_mpi<type_list<Ys...>, type_list<Zs...>>> {
  using type = typed_mpi<type_list<Xs...>, type_list<Zs...>>;
};

template <class X, class Y>
struct mpi_sequencer_all;

template <class X, class... Ys>
struct mpi_sequencer_all<X, type_list<Ys...>> {
  using type = type_list<typename mpi_sequencer_one<X, Ys>::type...>;
};

template <template <class...> class Target, class Ys, class... Xs>
struct mpi_sequencer {
  // combine each X with all Ys
  using all =
    typename tl_concat<
      typename mpi_sequencer_all<Xs, Ys>::type...
    >::type;
  // drop all mismatches (void results)
  using filtered = typename tl_filter_not_type<all, void>::type;
  // raise error if we don't have a single match
  static_assert(tl_size<filtered>::value > 0,
                "Left-hand actor type does not produce a single result which "
                "is valid as input to the right-hand actor type.");
  // compute final actor type
  using type = typename tl_apply<filtered, Target>::type;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_MPI_SEQUENCER_HPP
