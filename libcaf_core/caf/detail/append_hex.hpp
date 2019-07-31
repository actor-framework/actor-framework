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

#include <string>
#include <type_traits>

#include "caf/detail/type_traits.hpp"

namespace caf {
namespace detail {

void append_hex(std::string& result, const uint8_t* xs, size_t n);

template <class T>
enable_if_t<has_data_member<T>::value> append_hex(std::string& result,
                                                  const T& x) {
  return append_hex(result, reinterpret_cast<const uint8_t*>(x.data()),
                    x.size());
}

template <class T>
enable_if_t<std::is_integral<T>::value> append_hex(std::string& result,
                                                   const T& x) {
  return append_hex(result, reinterpret_cast<const uint8_t*>(&x), sizeof(T));
}

} // namespace detail
} // namespace caf
