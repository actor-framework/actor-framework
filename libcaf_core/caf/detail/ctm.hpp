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

#ifndef CAF_DETAIL_CTM_HPP
#define CAF_DETAIL_CTM_HPP

#include "caf/replies_to.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/typed_actor_util.hpp"

namespace caf {
namespace detail {

// CTM: Compile-Time Match

// left hand side is the MPI we are comparing to, this is *not* commutative
template <class A, class B>
struct ctm_cmp : std::false_type { };

template <class T>
struct ctm_cmp<T, T> : std::true_type { };

template <class In, class Out>
struct ctm_cmp<typed_mpi<In, Out, empty_type_list>,
               typed_mpi<In, type_list<typed_continue_helper<Out>>, empty_type_list>>
    : std::true_type { };

template <class In, class L, class R>
struct ctm_cmp<typed_mpi<In, L, R>,
               typed_mpi<In, type_list<skip_message_t>, empty_type_list>>
    : std::true_type { };

template <class A, class B>
struct ctm : std::false_type { };

template <>
struct ctm<empty_type_list, empty_type_list> : std::true_type { };

template <class A, class... As, class... Bs>
struct ctm<type_list<A, As...>, type_list<Bs...>>
    : std::conditional<
        sizeof...(As) + 1 != sizeof...(Bs),
        std::false_type,
        ctm<type_list<As...>,
          typename tl_filter_not<
            type_list<Bs...>,
            tbind<ctm_cmp, A>::template type
          >::type
        >
      >::type { };

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_CTM_HPP
