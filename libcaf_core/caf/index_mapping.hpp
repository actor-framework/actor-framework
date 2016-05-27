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

#ifndef CAF_INDEX_MAPPING_HPP
#define CAF_INDEX_MAPPING_HPP

#include <tuple>
#include <string>
#include <functional>

#include "caf/deep_to_string.hpp"

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

template <class Processor>
void serialize(Processor& proc, index_mapping& x, const unsigned int) {
  proc & x.value;
}

inline std::string to_string(const index_mapping& x) {
  return "idx" + deep_to_string_as_tuple(x.value);
}

} // namespace caf

#endif // CAF_INDEX_MAPPING_HPP
