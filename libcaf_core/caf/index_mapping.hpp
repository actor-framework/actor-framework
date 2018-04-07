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

#include <tuple>
#include <string>
#include <functional>

#include "caf/meta/type_name.hpp"

namespace caf {

/// Marker for representing placeholders at runtime.
struct index_mapping {
  int value;

  explicit index_mapping(int x) : value(x) {
    // nop
  }

  template <class T,
            class E = typename std::enable_if<
                        std::is_placeholder<T>::value != 0
                      >::type>
  index_mapping(T) : value(std::is_placeholder<T>::value) {
    // nop
  }
};

inline bool operator==(const index_mapping& x, const index_mapping& y) {
  return x.value == y.value;
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, index_mapping& x) {
  return f(meta::type_name("index_mapping"), x.value);
}

} // namespace caf

