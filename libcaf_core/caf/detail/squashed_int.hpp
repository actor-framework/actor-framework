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

namespace caf::detail {

/// Compile-time list of integer types types.
template <size_t>
struct int_types_by_size;

template <>
struct int_types_by_size<1> {
  using signed_type = int8_t;
  using unsigned_type = uint8_t;
};

template <>
struct int_types_by_size<2> {
  using signed_type = int16_t;
  using unsigned_type = uint16_t;
};

template <>
struct int_types_by_size<4> {
  using signed_type = int32_t;
  using unsigned_type = uint32_t;
};

template <>
struct int_types_by_size<8> {
  using signed_type = int64_t;
  using unsigned_type = uint64_t;
};

/// Squashes integer types into [u]int_[8|16|32|64]_t equivalents.
template <class T>
struct squashed_int {
  using tpair = int_types_by_size<sizeof(T)>;
  using type = std::conditional_t<std::is_signed<T>::value,    //
                                  typename tpair::signed_type, //
                                  typename tpair::unsigned_type>;
};

template <class T>
using squashed_int_t = typename squashed_int<T>::type;

template <class T, bool = std::is_integral<T>::value>
struct squash_if_int {
  using type = T;
};

template <>
struct squash_if_int<bool, true> {
  // Exempt bool from the squashing, since we treat it differently.
  using type = bool;
};

template <class T>
struct squash_if_int<T, true> {
  using type = squashed_int_t<T>;
};

template <class T>
using squash_if_int_t = typename squash_if_int<T>::type;

} // namespace caf::detail
