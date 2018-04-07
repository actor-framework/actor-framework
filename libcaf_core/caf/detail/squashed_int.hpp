/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <cstdint>
#include <type_traits>

#include "caf/detail/type_list.hpp"

namespace caf {
namespace detail {

/// Compile-time list of integer types types.
using int_types_by_size =
  detail::type_list<                      // bytes
    void,                                 // 0
    detail::type_pair<int8_t, uint8_t>,   // 1
    detail::type_pair<int16_t, uint16_t>, // 2
    void,                                 // 3
    detail::type_pair<int32_t, uint32_t>, // 4
    void,                                 // 5
    void,                                 // 6
    void,                                 // 7
    detail::type_pair<int64_t, uint64_t>  // 8
  >;

/// Squashes integer types into [u]int_[8|16|32|64]_t equivalents
template <class T>
struct squashed_int {
  using tpair = typename detail::tl_at<int_types_by_size, sizeof(T)>::type;
  using type = 
    typename std::conditional<
      std::is_signed<T>::value,
      typename tpair::first,
      typename tpair::second
    >::type;
};

template <class T>
using squashed_int_t = typename squashed_int<T>::type;

} // namespace detail
} // namespace caf


