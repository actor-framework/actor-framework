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

#include <stdexcept>
#include <type_traits>

#include "caf/unit.hpp"
#include "caf/none.hpp"

#define CAF_VARIANT_DATA_CONCAT(x, y) x ## y

#define CAF_VARIANT_DATA_GETTER(pos)                                           \
  inline CAF_VARIANT_DATA_CONCAT(T, pos) &                                     \
  get(std::integral_constant<int, pos>) {                                      \
    return CAF_VARIANT_DATA_CONCAT(v, pos);                                    \
  }                                                                            \
  inline const CAF_VARIANT_DATA_CONCAT(T, pos) &                               \
  get(std::integral_constant<int, pos>) const {                                \
    return CAF_VARIANT_DATA_CONCAT(v, pos);                                    \
  }

namespace caf {
namespace detail {

template <class T0 = unit_t, class T1 = unit_t, class T2 = unit_t,
          class T3 = unit_t, class T4 = unit_t, class T5 = unit_t,
          class T6 = unit_t, class T7 = unit_t, class T8 = unit_t,
          class T9 = unit_t, class T10 = unit_t, class T11 = unit_t,
          class T12 = unit_t, class T13 = unit_t, class T14 = unit_t,
          class T15 = unit_t, class T16 = unit_t, class T17 = unit_t,
          class T18 = unit_t, class T19 = unit_t, class T20 = unit_t,
          class T21 = unit_t, class T22 = unit_t, class T23 = unit_t,
          class T24 = unit_t, class T25 = unit_t, class T26 = unit_t,
          class T27 = unit_t, class T28 = unit_t, class T29 = unit_t>
struct variant_data {
  union {
    T0 v0;
    T1 v1;
    T2 v2;
    T3 v3;
    T4 v4;
    T5 v5;
    T6 v6;
    T7 v7;
    T8 v8;
    T9 v9;
    T10 v10;
    T11 v11;
    T12 v12;
    T13 v13;
    T14 v14;
    T15 v15;
    T16 v16;
    T17 v17;
    T18 v18;
    T19 v19;
    T20 v20;
    T21 v21;
    T22 v22;
    T23 v23;
    T24 v24;
    T25 v25;
    T26 v26;
    T27 v27;
    T28 v28;
    T29 v29;
  };

  variant_data() {
    // nop
  }

  ~variant_data() {
    // nop
  }

  CAF_VARIANT_DATA_GETTER(0)
  CAF_VARIANT_DATA_GETTER(1)
  CAF_VARIANT_DATA_GETTER(2)
  CAF_VARIANT_DATA_GETTER(3)
  CAF_VARIANT_DATA_GETTER(4)
  CAF_VARIANT_DATA_GETTER(5)
  CAF_VARIANT_DATA_GETTER(6)
  CAF_VARIANT_DATA_GETTER(7)
  CAF_VARIANT_DATA_GETTER(8)
  CAF_VARIANT_DATA_GETTER(9)
  CAF_VARIANT_DATA_GETTER(10)
  CAF_VARIANT_DATA_GETTER(11)
  CAF_VARIANT_DATA_GETTER(12)
  CAF_VARIANT_DATA_GETTER(13)
  CAF_VARIANT_DATA_GETTER(14)
  CAF_VARIANT_DATA_GETTER(15)
  CAF_VARIANT_DATA_GETTER(16)
  CAF_VARIANT_DATA_GETTER(17)
  CAF_VARIANT_DATA_GETTER(18)
  CAF_VARIANT_DATA_GETTER(19)
  CAF_VARIANT_DATA_GETTER(20)
  CAF_VARIANT_DATA_GETTER(21)
  CAF_VARIANT_DATA_GETTER(22)
  CAF_VARIANT_DATA_GETTER(23)
  CAF_VARIANT_DATA_GETTER(24)
  CAF_VARIANT_DATA_GETTER(25)
  CAF_VARIANT_DATA_GETTER(26)
  CAF_VARIANT_DATA_GETTER(27)
  CAF_VARIANT_DATA_GETTER(28)
  CAF_VARIANT_DATA_GETTER(29)
};

struct variant_data_destructor {
  using result_type = void;

  template <class T>
  void operator()(T& storage) const {
    storage.~T();
  }
};

} // namespace detail
} // namespace caf

