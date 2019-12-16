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

#include <array>
#include <cstdint>
#include <typeinfo>

#include "caf/detail/core_export.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/fwd.hpp"
#include "caf/type_nr.hpp"

namespace caf::detail {

struct meta_element {
  uint16_t typenr;
  const std::type_info* type;
};

template <class T, uint16_t TN = type_nr<T>::value>
struct meta_element_factory {
  static meta_element create() {
    return {TN, nullptr};
  }
};

template <class T>
struct meta_element_factory<T, 0> {
  static meta_element create() {
    return {0, &typeid(T)};
  }
};

template <class TypeList>
struct meta_elements;

template <class... Ts>
struct meta_elements<type_list<Ts...>> {
  std::array<meta_element, sizeof...(Ts)> arr;
  meta_elements() : arr{{meta_element_factory<Ts>::create()...}} {
    // nop
  }
};

} // namespace caf::detail
